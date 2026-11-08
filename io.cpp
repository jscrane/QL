#include <cstdint>

#include <machine.h>
#include <memory.h>
#include <debugging.h>

#include "microdrive.h"
#include "ipc.h"
#include "io.h"

bool debug_io = false;

void IO::operator=(uint8_t b) {

	if (debug_io) DBG_EMU("> IO: %06x %02x", _acc, b);

	switch (_acc) {
	case DISPLAY_CTRL:
		if (_display_ctrl)
			_display_ctrl(b);
		break;
	case IPC_LINK_CTRL:
		_ipc.write_link(b);
		break;
	case INT_CTRL:
		interrupt_acknowledge(b);
		break;
	case MDV_CTRL:
		_mdv.write_control(b);
		break;
	case TRANSMIT_CTRL:
		// ignore for now...
		break;
	default:
		DBG_EMU("> IO: unimplemented %06x %02x", _acc, b);
		break;
	}
}

IO::operator uint8_t() {

	uint8_t b = 0;

	switch (_acc) {
	case INT_IPC_STAT:
		b = _irq_status;
		break;
	case MDV_RS_STAT:
		if (_ipc.comdata())
			b |= 0x80;
		break;
	case RTC_BYTE0:
		b = (_rtc >> 24) & 0xff;
		break;
	case RTC_BYTE1:
		b = (_rtc >> 16) & 0xff;
		break;
	case RTC_BYTE2:
		b = (_rtc >> 8) & 0xff;
		break;
	case RTC_BYTE3:
		b = _rtc & 0xff;
		break;
	default:
		DBG_EMU("< IO: unimplemented %06x", _acc);
		break;
	}

	if (debug_io) DBG_EMU("< IO: %06x %02x", _acc, b);

	return b;
}

void IO::poll() {

	if (_ipc.pending())
		_irq_status |= INT_INTERFACE;

	if (_mdv.gap())
		_irq_status |= INT_GAP;

	bool frame = (_irq_status & INT_FRAME);
	bool ipc = (_irq_status & INT_INTERFACE) && (_irq_mask & MASK_INTERFACE);
	bool gap = (_irq_status & INT_GAP) && (_irq_mask & MASK_GAP);

	if (_interrupt_cpu)
		_interrupt_cpu(frame || ipc || gap);
}

void IO::interrupt_acknowledge(uint8_t b) {

	// bits 7-5 are the interrupt mask, bits 4-0 clear pending interrupts
	_irq_mask = b & 0xe0;
	_irq_status &= ~(b & 0x1f);
}

void IO::begin() {

	_machine->interval_timer(20000, [this]() { _irq_status |= INT_FRAME; });
	_machine->interval_timer(1000000, [this]() { _rtc++; });
}

void IO::reset() {

	_mdv.reset();
	_ipc.reset();
	_irq_status = _irq_mask = 0;
	_rtc = 0;
}

void IO::checkpoint(Checkpoint &c) {
	// FIXME
}

void IO::restore(Checkpoint &c) {
	// FIXME
}
