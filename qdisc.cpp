#include <cstdint>

#include <machine.h>
#include <debugging.h>
#include <memory.h>

#include "qdisc.h"

// QDisc has a WD1770 at $c3ff0
#define WD_BASE		0xf0
#define WD_COMMAND	WD_BASE
#define WD_STATUS	WD_BASE
#define WD_TRACK	(WD_BASE + 2)		// QDisc wiring: address lines are reversed
#define WD_SECTOR	(WD_BASE + 1)
#define WD_DATA		(WD_BASE + 3)

#define LATCH		0xf8
#define LA_HEAD		0x01
#define LA_DISC_0	0x02
#define LA_DISC_1	0x04
#define LA_DENSITY	0x08

void QDisc::operator=(uint8_t b) {

	DBG_DISK("> QDisc: %02x %02x", _acc, b);

	switch (_acc) {
	case WD_COMMAND:
		_wd.write_command(b);
		break;
	case WD_TRACK:
		DBG_DISK("> QDisc: track=%02x", b);
		_wd.write_track(b);
		break;
	case WD_SECTOR:
		DBG_DISK("> QDisc: sector=%02x", b);
		_wd.write_sector(b);
		break;
	case WD_DATA:
		DBG_DISK("> QDisc: data=%02x", b);
		_wd.write_data(b);
		break;
	case LATCH:
		DBG_DISK("> QDisc: latch=%02x", b);
		write_latch(b);
		break;
	}
}

QDisc::operator uint8_t() {

	uint8_t b = 0xff;

	switch (_acc) {
	case WD_STATUS:
		b = _wd.read_status();
		break;
	case WD_TRACK:
		b = _wd.read_track();
		break;
	case WD_SECTOR:
		b = _wd.read_sector();
		break;
	case WD_DATA:
		b = _wd.read_data();
		break;
	}

	return b;
}

void QDisc::write_latch(uint8_t b) {

	if (b != _latch) {
		_wd.set_side(b & LA_HEAD? 1: 0);
		_wd.set_drive(b & LA_DISC_0? 0: 1);
		_latch = b;
	}
}
