#include <cstdint>

#include <machine.h>
#include <debugging.h>
#include <memory.h>
#include <compat.h>
#include <prom.h>

#include "qdisc.h"

#include <CPU.h>
#include <m68k.h>
extern m68k cpu;

void QDisc::operator=(uint8_t b) {

	DBG_EMU("> QDisc: %06x %04x %02x", cpu.base_pc(), _acc, b);
}

QDisc::operator uint8_t() {

	uint8_t b = 0;

	DBG_EMU("< QDisc: %06x %04x %02x", cpu.base_pc(), _acc, b);
	return b;
}

void QDisc::checkpoint(Checkpoint &) {
	// FIXME
}

void QDisc::restore(Checkpoint &) {
	// FIXME
}
