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

// commands
#define WD_RESTORE	0x00	// type-1
#define WD_SEEK		0x10
#define WD_STEP		0x20
#define WD_STEP_IN	0x40
#define WD_STEP_OUT	0x60
#define WD_READ_SECTOR	0x80	// type-2
#define WD_WRITE_SECTOR	0xa0
#define WD_READ_DAM	0xc0	// type-3
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

// latch
#define LATCH		0xf8
#define LA_HEAD		0x01
#define LA_DISC_0	0x02
#define LA_DISC_1	0x04
#define LA_DENSITY	0x08

static inline uint8_t cmd_type(uint8_t b) { return b & 0xf0; }

static inline uint8_t cmd_flags(uint8_t b) { return b & 0x0f; }

static uint8_t last_command = 0;

void QDisc::operator=(uint8_t b) {

	DBG_EMU("> QDisc: %06x %04x %02x", cpu.base_pc(), _acc, b);

	switch (_acc) {
	case WD_COMMAND:
		write_command(b);
		break;
	case WD_TRACK:
		DBG_EMU("> QDisc: track=%02x", b);
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

	if (cpu.base_pc() != 0x0c13d6)
		DBG_EMU("< QDisc: %06x %04x %02x cmd=%02x", cpu.base_pc(), _acc, b, last_command);

	return b;
}

void QDisc::write_command(uint8_t b) {

	_wd_status |= WD_BUSY;

	switch (cmd_type(b)) {
	case WD_RESTORE:
		_wd_status |= WD_INDEX;
		_wd_status &= ~WD_BUSY;
		_wd_index_reads = 0;
		break;
	case WD_SEEK:
		_wd_track = _wd_data;
		_wd_status &= ~WD_BUSY;
		DBG_EMU("QDisc: SEEK track=%02x", _wd_track);
		break;
	case WD_STEP_IN:
		break;
	case WD_STEP_OUT:
		break;
	case WD_FORCE_INT:
		_wd_status &= ~WD_BUSY;
		break;
	case WD_READ_SECTOR:
		DBG_EMU("QDisc: SECTOR track=%02x sector=%02x", _wd_track, _wd_sector);

		_image.seek(SECTOR_BYTES*((_wd_sector-1) + TRACK_SECTORS*_wd_track));
		_wd_sector_pos = 0;
		_wd_status = WD_BUSY | WD_DRQ;
		_wd_read_active = true;
		break;
	}

	last_command = b;
}

uint8_t QDisc::read_status() {

	if (_wd_index_reads++ > 10)
		_wd_status &= ~WD_INDEX;

	uint8_t b = _wd_status;
	if (_image)
		b &= ~WD_NOT_READY;
	else
		b |= WD_NOT_READY;

	return b;
}

uint8_t QDisc::read_data() {

	uint8_t b = 0xff;

	if (!_wd_read_active)
		return b;

	int data = _image.read();
	if (data < 0) {
		_wd_status &= ~(WD_BUSY | WD_DRQ);
		_wd_status |= WD_CRC_ERR;	// temporary read-failure indication
		_wd_read_active = false;
		return b;
	}

	b = (uint8_t)data;
	_wd_sector_pos++;

	if (_wd_sector_pos >= SECTOR_BYTES) {
		_wd_status &= ~(WD_BUSY | WD_DRQ);
		_wd_read_active = false;
	}

	DBG_EMU("< QDisc DATA pos=%d data=%02x", _wd_sector_pos, b);
	return b;
}

void QDisc::reset() {

	// FIXME: more
	_wd_sector = 1;
	_wd_sector_pos = 0;
	_wd_read_active = false;
}

void QDisc::checkpoint(Checkpoint &) {
	// FIXME
}

void QDisc::restore(Checkpoint &) {
	// FIXME
}
