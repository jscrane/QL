//
// https://www.worldofsam.org/node/1097
// https://github.com/MisterTea/MAMEHub/blob/master/Sources/Emulator/src/emu/machine/wd17xx.c
//
#include <cstdint>

#include <machine.h>
#include <debugging.h>
#include <memory.h>
#include <compat.h>
#include <flash_filer.h>

#include "wd1770.h"

#define TRACKS		80
#define MAX_TRACK	(TRACKS-1)		// tracks are numbered from 0
#define TRACK_SECTORS	9			// 9 sectors per track
#define MAX_SECTOR	TRACK_SECTORS		// sectors are numbered from 1
#define SECTOR_BYTES	512			// 512 bytes per sector

#define WD_STEP_UPDATE	0x10

// commands
#define WD_RESTORE	0x00	// type-1
#define WD_SEEK		0x10
#define WD_STEP		0x20
#define WD_STEP_U	(WD_STEP + WD_STEP_UPDATE)
#define WD_STEP_IN	0x40
#define WD_STEP_IN_U	(WD_STEP_IN + WD_STEP_UPDATE)
#define WD_STEP_OUT	0x60
#define WD_STEP_OUT_U	(WD_STEP_OUT + WD_STEP_UPDATE)
#define WD_READ_SECT	0x80	// type-2
#define WD_READ_SECT_M	0x90
#define WD_WRITE_SECT	0xa0
#define WD_WRITE_SECT_M	0xb0
#define WD_READ_ADDR	0xc0	// type-3
#define WD_READ_TRACK	0xe0
#define WD_WRITE_TRACK	0xf0
#define WD_FORCE_INT	0xd0	// type-4

// status flags
#define WD_BUSY		0x01	// type-1
#define WD_INDEX	0x02
#define WD_TRACK0	0x04
#define WD_CRC_ERR	0x08
#define WD_SEEK_ERR	0x10
#define WD_HD_LOADED	0x20
#define WD_WRITE_PROT	0x40
#define WD_MOTOR_ON	0x80	// wd1770 / wd1772 only
#define WD_DRQ		0x02	// type-2
#define WD_REC_NT_FND	0x10
#define WD_WRITE_FAULT	0x20

static inline uint8_t cmd(uint8_t b) { return b & 0xf0; }

static inline uint8_t cmd_type(uint8_t cmd) {
	if (!(cmd & 0x80)) return 1;
	if (!(cmd & 0x40)) return 2;
	if (!(cmd & 0x10)) return 3;
	return (cmd & 0x20)? 3: 4;
}

static inline uint8_t cmd_flags(uint8_t b) { return b & 0x0f; }

bool wd1770::seek() {
	if (_sector >= 1 && _sector <= MAX_SECTOR && _track <= MAX_TRACK && *_image) {
		uint32_t offset = SECTOR_BYTES*((_sector-1) + TRACK_SECTORS*(_track*2 + _side));
		return _image->seek(offset);
	}
	return false;
}

void wd1770::write_command(uint8_t b) {

	uint8_t c = cmd(b);
	_command_type = cmd_type(c);

	if (c != WD_FORCE_INT)
		_status &= ~(WD_CRC_ERR | WD_REC_NT_FND | WD_WRITE_FAULT);

	switch (c) {
	case WD_RESTORE:
		_status &= ~WD_BUSY;
		_index_reads = 0;
		_track = 0;
		_direction = -1;
		break;
	case WD_SEEK:
		_track = _data;
		_status &= ~WD_BUSY;
		DBG_DISK("wd1770: SEEK track=%02x", _track);
		break;
	case WD_STEP:
	case WD_STEP_U:
		if (c & WD_STEP_UPDATE) {
			if (_track > 0 && _direction < 0)
				_track--;
			else if (_track < MAX_TRACK && _direction > 0)
				_track++;
		}
		_status &= ~WD_BUSY;
		break;
	case WD_STEP_IN:
	case WD_STEP_IN_U:
		_direction = 1;
		if ((c & WD_STEP_UPDATE) && _track < MAX_TRACK)
			_track++;
		_status &= ~WD_BUSY;
		break;
	case WD_STEP_OUT:
	case WD_STEP_OUT_U:
		_direction = -1;
		if ((c & WD_STEP_UPDATE) && _track > 0)
			_track--;
		_status &= ~WD_BUSY;
		break;
	case WD_FORCE_INT:
		_read_active = _write_active = false;
		_status &= ~(WD_BUSY | WD_DRQ | WD_CRC_ERR | WD_REC_NT_FND | WD_WRITE_FAULT);
		break;
	case WD_READ_SECT:
	case WD_READ_SECT_M:
		_multi = (c == WD_READ_SECT_M);
		DBG_DISK("wd1770: READ SECTOR%s track=%02x sector=%02x", _multi? " (multi)": "", _track, _sector);
		_read_active = seek();
		if (_read_active) {
			_sector_pos = 0;
			_status = WD_BUSY | WD_DRQ;
		} else {
			if (*_image) DBG_DISK("wd1770: seek failed");
			_status &= ~(WD_BUSY | WD_DRQ);
			_status |= WD_REC_NT_FND;
		}
		break;
	case WD_WRITE_SECT:
	case WD_WRITE_SECT_M:
		_multi = (c == WD_WRITE_SECT_M);
		DBG_DISK("wd1770: WRITE SECTOR%s track=%02x sector=%02x", _multi? " (multi)": "", _track, _sector);
		_write_active = seek();
		if (_write_active) {
			_sector_pos = 0;
			_status = WD_BUSY | WD_DRQ;
		} else {
			if (*_image) DBG_DISK("wd1770: seek failed");
			_status &= ~(WD_BUSY | WD_DRQ);
			_status |= WD_REC_NT_FND;
		}
		break;
	case WD_READ_ADDR:
		DBG_DISK("wd1770: READ ADDR status=%02x", _status);
		_sector = _track;
		_status &= ~WD_BUSY;
		break;
	}
}

uint8_t wd1770::read_status() {

	uint8_t b = _status | WD_MOTOR_ON;

	if (_command_type == 1 || _command_type == 4) {

		// pulse for 32 reads out of 4096
		b &= ~WD_INDEX;
		if ((_index_reads++ & 0x0fff) < 32)
			b |= WD_INDEX;

		if (_track == 0)
			b |= WD_TRACK0;
		else
			b &= ~WD_TRACK0;
	}

	return b;
}

bool wd1770::next_sector() {

	if (_multi) {
		_sector++;
		if (seek()) {
			_sector_pos = 0;
			return true;
		}
		_status |= WD_REC_NT_FND;	// off the end of the track: normal termination
	}

	_status &= ~(WD_BUSY | WD_DRQ);
	return false;
}

uint8_t wd1770::read_data() {

	if (!_read_active)
		return 0xff;

	int data = _image->read();
	if (data < 0) {
		DBG_DISK("read_data: %d: %d", _sector_pos, (bool)*_image);
		_status &= ~(WD_BUSY | WD_DRQ);
		_status |= WD_CRC_ERR;		// temporary read-failure indication
		_read_active = false;
		return 0xff;
	}

	uint8_t b = (uint8_t)data;
	_sector_pos++;

	if (_sector_pos >= SECTOR_BYTES)
		_read_active = next_sector();

	return b;
}

void wd1770::write_data(uint8_t b) {

	_data = b;

	if (!_write_active) return;

	size_t n = _image->write(b);
	if (n < 1) {
		DBG_DISK("write_data: %d: %02x (%d) %d", _sector_pos, b, n, (bool)*_image);
		_status &= ~(WD_BUSY | WD_DRQ);
		_status |= WD_WRITE_FAULT;
		_write_active = false;
		return;
	}

	_sector_pos++;

	if (_sector_pos >= SECTOR_BYTES) {
		_image->flush();
		_write_active = next_sector();
	}
}

void wd1770::reset() {

	_sector = 1;
	_sector_pos = 0;
	_read_active = _write_active = _multi = false;
	_command_type = 0;
	_track = 0;
	_direction = -1;
}

void wd1770::set_drive(uint8_t b) {

	flash_file *f = b? &_d2: &_d1;
	if (f != _image) {
		_image = f;
		_read_active = _write_active = false;
		_status &= ~(WD_BUSY | WD_DRQ);
	}
}
