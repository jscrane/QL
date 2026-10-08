#include <cstdint>

#include <machine.h>
#include <debugging.h>
#include <memory.h>
#include <compat.h>
#include <prom.h>

#include "qdisc.h"

// debugging
#include <CPU.h>
#include <m68k.h>
extern m68k cpu;

// QDisc has a WD1770 at $c3ff0
// https://github.com/fesh0r/old-mame/blob/master/src/emu/machine/wd17xx.c
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
#define WD_BUSY		0x01
#define WD_INDEX	0x02
#define WD_TRACK0	0x04
#define WD_CRC_ERR	0x08
#define WD_SEEK_ERR	0x10
#define WD_HD_LOADED	0x20
#define WD_WRITE_PROT	0x40
#define WD_NOT_READY	0x80

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
			_wd_status = WD_NOT_READY;
			break;
		}
		last_command = b;
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

	uint8_t b = 0;

	switch (_acc) {
	case WD_STATUS:
		if (_wd_index_reads++ > 10)
			_wd_status &= ~WD_INDEX;

		b = _wd_status;
		break;
	case WD_TRACK:
		b = _wd_track;
		break;
	case WD_SECTOR:
		b = _wd_sector;
		break;
	case WD_DATA:
		b = _wd_data;
		break;
	}

	DBG_EMU("< QDisc: %06x %04x %02x cmd=%02x", cpu.base_pc(), _acc, b, last_command);

	return b;
}

void QDisc::checkpoint(Checkpoint &) {
	// FIXME
}

void QDisc::restore(Checkpoint &) {
	// FIXME
}
