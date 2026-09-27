#include <cstdint>

#include <machine.h>
#include <memory.h>
#include <debugging.h>

#include "keyboard.h"
#include "microdrive.h"
#include "io.h"

bool debug_io = false;

void IO::operator=(uint8_t b) {

	if (debug_io) DBG_EMU("> %06x %02x", _acc, b);

	switch (_acc) {
	case DISPLAY_CTRL:
		if (_display_ctrl)
			_display_ctrl(b);
		break;
	case IPC_LINK_CTRL:
		if (_ipc_state == IPCState::Start) {
			if (b == 0x01)
				_ipc_state = IPCState::Command;
		} else if (_ipc_state == IPCState::Command)
			receive_command_bit(b);
		else if (b == 0x0e)
			send_response_bit(b);
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
		DBG_EMU("> %06x %02x", _acc, b);
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
		b = _comdata;
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
		DBG_EMU("< %06x", _acc);
		break;
	}

	if (debug_io) DBG_EMU("< %06x %02x", _acc, b);

	return b;
}

void IO::receive_command_bit(uint8_t b) {

	if (debug_io) DBG_EMU("cmd bit %d: %02x", _cmd_bits, b);
	_cmd = (_cmd << 1) + (b == 0x0e);
	if (++_cmd_bits == 4) {
		uint8_t cmd = _cmd;
		_cmd = 0;
		_cmd_bits = 0;
		link_command(cmd);
	}
}

void IO::send_response_bit(uint8_t b) {

	bool bit = _rsp & _rsp_bit;
	if (debug_io) DBG_EMU("rsp %d %02x", bit, _rsp_bit);
	_comdata = bit? 0x80: 0x00;
	_rsp_bit >>= 1;
	if (_rsp_bit == 0)
		_ipc_state = IPCState::Command;
}

void IO::link_command(uint8_t cmd) {

	if (debug_io) DBG_EMU("link_command %02x", cmd);

	_ipc_state = IPCState::Response;

	switch (cmd) {
	case 1:
		// response is 8 bits
		_rsp_bit = 0x0080;
		_rsp = 0x40 | _kbd.available();
		break;
	case 8:
		if (_kbd.available()) {
			// response is 16 bits
			_rsp_bit = 0x8000;
			_rsp = (1 << 12) | _kbd.read();
		} else {
			// response is 4 bits
			_rsp_bit = 0x0008;
			_rsp = 0;
		}
		break;
	default:
		DBG_EMU("unimplemented link_command %02x", cmd);
		break;
	}
}

void IO::poll() {

	if (_kbd.available())
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

void IO::checkpoint(Checkpoint &c) {
	// FIXME
}

void IO::restore(Checkpoint &c) {
	// FIXME
}
