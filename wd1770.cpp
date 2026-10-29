#include <cstdint>

#include <machine.h>
#include <debugging.h>
#include <memory.h>
#include <compat.h>
#include <serialio.h>
#include <filer.h>
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
#define WD_NOT_READY	0x80
#define WD_DRQ		0x02	// type-2
#define WD_REC_NT_FND	0x10

static inline uint8_t cmd(uint8_t b) { return b & 0xf0; }

static inline uint8_t cmd_type(uint8_t cmd) {
	if (!(cmd & 0x80)) return 1;
	if (!(cmd & 0x40)) return 2;
	if (!(cmd & 0x10)) return 3;
	return (cmd & 0x20)? 3: 4;
}

static inline uint8_t cmd_flags(uint8_t b) { return b & 0x0f; }

inline uint32_t wd1770::offset() const {
	return SECTOR_BYTES*((_wd_sector-1) + TRACK_SECTORS*(_wd_track*2 + _side));
}

inline bool wd1770::can_seek() const {
	return _wd_sector >= 1 && _wd_sector <= MAX_SECTOR && _wd_track <= MAX_TRACK && *_image;
}

void wd1770::write_command(uint8_t b) {

	uint8_t c = cmd(b);
	_wd_command_type = cmd_type(c);

	switch (c) {
	case WD_RESTORE:
		_wd_status |= WD_INDEX;
		_wd_status &= ~WD_BUSY;
		_wd_index_reads = 0;
		_wd_track = 0;
		_wd_direction = -1;
		break;
	case WD_SEEK:
		_wd_track = _wd_data;
		_wd_status &= ~WD_BUSY;
		DBG_DISK("wd1770: SEEK track=%02x", _wd_track);
		break;
	case WD_STEP:
	case WD_STEP_U:
		if (c & WD_STEP_UPDATE) {
			if (_wd_track > 0 && _wd_direction < 0)
				_wd_track--;
			else if (_wd_track < MAX_TRACK && _wd_direction > 0)
				_wd_track++;
		}
		_wd_status &= ~WD_BUSY;
		break;
	case WD_STEP_IN:
	case WD_STEP_IN_U:
		_wd_direction = 1;
		if ((c & WD_STEP_UPDATE) && _wd_track < MAX_TRACK)
			_wd_track++;
		_wd_status &= ~WD_BUSY;
		break;
	case WD_STEP_OUT:
	case WD_STEP_OUT_U:
		_wd_direction = -1;
		if ((c & WD_STEP_UPDATE) && _wd_track > 0)
			_wd_track--;
		_wd_status &= ~WD_BUSY;
		break;
	case WD_FORCE_INT:
		_wd_read_active = false;
		_wd_status &= ~(WD_BUSY | WD_DRQ);
		break;
	case WD_READ_SECT:
		DBG_DISK("wd1770: READ SECTOR track=%02x sector=%02x", _wd_track, _wd_sector);
		_wd_read_active = can_seek() && _image->seek(offset());
		if (_wd_read_active) {
			_wd_sector_pos = 0;
			_wd_status = WD_BUSY | WD_DRQ;
		} else {
			if (*_image) DBG_DISK("wd1770: seek failed");
			_wd_status &= ~(WD_BUSY | WD_DRQ);
			_wd_status |= WD_REC_NT_FND;
		}
		break;
	case WD_WRITE_SECT:
		DBG_DISK("wd1770: WRITE SECTOR track=%02x sector=%02x", _wd_track, _wd_sector);
		_wd_status |= WD_WRITE_PROT;
		_wd_status &= ~WD_BUSY;
		break;
	case WD_READ_ADDR:
		DBG_DISK("wd1770: ADDR status=%02x last=%02x", _wd_status, _wd_last_cmd);
		_wd_sector = _wd_track;
		_wd_status &= ~WD_BUSY;
		break;
	default:
		DBG_DISK("wd1770: unimplemented command %02x", c);
		break;
	}

	_wd_last_cmd = b;
}

uint8_t wd1770::read_status() {

	if (_wd_command_type == 1 && _wd_index_reads++ > 10)
		_wd_status &= ~WD_INDEX;

	uint8_t b = _wd_status;
	if (*_image)
		b &= ~WD_NOT_READY;
	else
		b |= WD_NOT_READY;

	if (_wd_command_type == 1 || _wd_command_type == 4) {
		if (_wd_track == 0)
			b |= WD_TRACK0;
		else
			b &= ~WD_TRACK0;
	}

	return b;
}

uint8_t wd1770::read_data() {

	if (!_wd_read_active)
		return 0xff;

	int data = _image->read();
	if (data < 0) {
		_wd_status &= ~(WD_BUSY | WD_DRQ);
		_wd_status |= WD_CRC_ERR;	// temporary read-failure indication
		_wd_read_active = false;
		return 0xff;
	}

	uint8_t b = (uint8_t)data;
	_wd_sector_pos++;

	if (_wd_sector_pos >= SECTOR_BYTES) {
		_wd_status &= ~(WD_BUSY | WD_DRQ);
		_wd_read_active = false;
	}

	if (_wd_sector_pos == 1 || _wd_sector_pos == SECTOR_BYTES)
		DBG_DISK("< wd1770 DATA pos=%d data=%02x", _wd_sector_pos, b);

	return b;
}

void wd1770::reset() {

	_wd_sector = 1;
	_wd_sector_pos = 0;
	_wd_read_active = false;
	_wd_command_type = 0;
	_wd_track = 0;
	_wd_direction = -1;
}

void wd1770::set_drive(uint8_t b) {

	_image = b? &_d2: &_d1;
	reset();
}
