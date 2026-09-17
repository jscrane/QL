#include <cstdint>

#include <machine.h>
#include <memory.h>
#include <debugging.h>

#include "io.h"

void IO::operator=(uint8_t b) {

	switch (_acc) {
	case DISPLAY_CTRL:
		if (_display_ctrl) {
			_display_ctrl(b);
			return;
		}
		break;
	}
	DBG_EMU("> %06x %02x", _acc, b);
}

IO::operator uint8_t() {
	DBG_EMU("< %06x", _acc);
	// FIXME
	return 0;
}

void IO::checkpoint(Checkpoint &c) {
	// FIXME
}

void IO::restore(Checkpoint &c) {
	// FIXME
}
