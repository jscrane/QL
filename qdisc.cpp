#include <cstdint>

#include <machine.h>
#include <debugging.h>
#include <memory.h>
#include <compat.h>
#include <prom.h>
#include <serialio.h>
#include <filer.h>
#include <flash_filer.h>

#include "qdisc.h"

// debugging
#include <CPU.h>
#include <m68k.h>
extern m68k cpu;
extern bool debug_cpu;

#define TRACK_SECTORS	9
#define SECTOR_BYTES	512

// QDisc has a WD1770 at $c3ff0
// https://github.com/MisterTea/MAMEHub/blob/master/Sources/Emulator/src/emu/machine/wd17xx.c
#define WD_BASE		0xf0
#define WD_COMMAND	WD_BASE
#define WD_STATUS	WD_BASE
#define WD_TRACK	(WD_BASE + 1)
#define WD_SECTOR	(WD_BASE + 2)
#define WD_DATA		(WD_BASE + 3)

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
	if (!(cmd & 0x01)) return 3;
	return 4;
}

static inline uint8_t cmd_flags(uint8_t b) { return b & 0x0f; }

// latch
#define LATCH		0xf8
#define LA_HEAD		0x01
#define LA_DISC_0	0x02
#define LA_DISC_1	0x04
#define LA_DENSITY	0x08

inline int QDisc::disk_side() const { return (_latch & LA_HEAD)? 1: 0; }

inline int QDisc::disk_drive() const { return (_latch & LA_DISC_0)? 0: 1; }

inline uint32_t QDisc::offset() const {
	return SECTOR_BYTES*((_wd_sector-1) + TRACK_SECTORS*((_wd_track-1)*2 + disk_side()));
}

void QDisc::operator=(uint8_t b) {

	DBG_EMU("> QDisc: %02x %02x %06x", _acc, b, cpu.base_pc());

	switch (_acc) {
	case WD_COMMAND:
		write_command(b);
		break;
	case WD_TRACK:
		DBG_EMU("> QDisc: track=%02x", b);
		if (b != 0xff)
			_wd_track = b;
		break;
	case WD_SECTOR:
		DBG_EMU("> QDisc: sector=%02x", b);
		_wd_sector = b;
		break;
	case WD_DATA:
		DBG_EMU("> QDisc: data=%02x", b);
		_wd_data = b;
		break;
	case LATCH:
		DBG_EMU("> QDisc: latch=%02x", b);
		_latch = b;
		break;
	}
}

QDisc::operator uint8_t() {

	uint8_t b = 0xff;

	switch (_acc) {
	case WD_STATUS:
		b = read_status();
		break;
	case WD_TRACK:
		b = _wd_track;
		break;
	case WD_SECTOR:
		b = _wd_sector;
		break;
	case WD_DATA:
		b = read_data();
		break;
	}

	if (cpu.base_pc() != 0x0c13d6 && cpu.base_pc() != 0x0c133e && b != 0xff)
		DBG_EMU("< QDisc: %02x %02x %06x cmd=%02x", _acc, b, cpu.base_pc(), _wd_last_cmd);

	return b;
}

void QDisc::write_command(uint8_t b) {

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
		DBG_EMU("QDisc: SEEK track=%02x", _wd_track);
		break;
	case WD_STEP:
	case WD_STEP_U:
		if (c & WD_STEP_UPDATE)
			_wd_track += _wd_direction;
		_wd_status &= ~WD_BUSY;
		break;
	case WD_STEP_IN:
	case WD_STEP_IN_U:
		_wd_direction = 1;
		if (c & WD_STEP_UPDATE)
			_wd_track += _wd_direction;
		_wd_status &= ~WD_BUSY;
		break;
	case WD_STEP_OUT:
	case WD_STEP_OUT_U:
		_wd_direction = -1;
		if (c & WD_STEP_UPDATE)
			_wd_track += _wd_direction;
		_wd_status &= ~WD_BUSY;
		break;
	case WD_FORCE_INT:
		_wd_status &= ~WD_BUSY;
		break;
	case WD_READ_SECT:
		DBG_EMU("QDisc: SECTOR track=%02x sector=%02x", _wd_track, _wd_sector);
		if (_image.seek(offset())) {
			_wd_sector_pos = 0;
			_wd_status = WD_BUSY | WD_DRQ;
			_wd_read_active = true;
		} else if (_image)
			DBG_EMU("QDisc: seek failed");
		break;
	case WD_READ_ADDR:
		DBG_EMU("QDisc: ADDR status=%02x last=%02x", _wd_status, _wd_last_cmd);
		_wd_sector = _wd_track;
		_wd_status &= ~WD_BUSY;
		break;
	default:
		DBG_EMU("QDisc: unimplemented command %02x", c);
		break;
	}

	_wd_last_cmd = b;
}

uint8_t QDisc::read_status() {

	if (_wd_command_type == 1 && _wd_index_reads++ > 10)
		_wd_status &= ~WD_INDEX;

	uint8_t b = _wd_status;
	if (_image)
		b &= ~WD_NOT_READY;
	else
		b |= WD_NOT_READY;

	return b;
}

uint8_t QDisc::read_data() {

	if (!_wd_read_active)
		return 0xff;

	int data = _image.read();
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
		DBG_EMU("< QDisc DATA pos=%d data=%02x", _wd_sector_pos, b);

	return b;
}

void QDisc::reset() {

	// FIXME: more
	_wd_sector = 1;
	_wd_sector_pos = 0;
	_wd_read_active = false;
	_wd_command_type = 0;
	_wd_track = 0;
	_wd_direction = -1;
}

void QDisc::checkpoint(Checkpoint &) {
	// FIXME
}

void QDisc::restore(Checkpoint &) {
	// FIXME
}
