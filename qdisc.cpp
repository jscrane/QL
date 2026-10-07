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
#define WD_BASE		0xf0
#define WD_COMMAND	WD_BASE
#define WD_STATUS	WD_BASE
#define WD_TRACK	(WD_BASE + 1)
#define WD_SECTOR	(WD_BASE + 2)
#define WD_DATA		(WD_BASE + 3)

// command types
#define WD_RESTORE	0x00

// status flags
#define WD_BUSY		0x01
#define WD_INDEX	0x02
#define WD_TRACK0	0x04
#define WD_CRC_ERR	0x08
#define WD_SEEK_ERR	0x10
#define WD_HD_LOADED	0x20
#define WD_WRITE_PROT	0x40
#define WD_NOT_READY	0x80

static inline uint8_t cmd_type(uint8_t b) { return b & 0xf0; }

static inline uint8_t cmd_flags(uint8_t b) { return b & 0x0f; }

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
		}
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
	}

	DBG_EMU("< QDisc: %06x %04x %02x", cpu.base_pc(), _acc, b);

	return b;
}

void QDisc::checkpoint(Checkpoint &) {
	// FIXME
}

void QDisc::restore(Checkpoint &) {
	// FIXME
}
