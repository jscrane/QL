#include <cstdint>

#include <machine.h>
#include <memory.h>
#include <debugging.h>
#include <CPU.h>
#include <m68k.h>

#include "keyboard.h"
#include "io.h"

void IO::operator=(uint8_t b) {

	if (_debugging) DBG_EMU("> %06x %02x", _acc, b);

	switch (_acc) {
	case DISPLAY_CTRL:
		if (_display_ctrl)
			_display_ctrl(b);
		break;
	case IPC_LINK_CTRL:
		if (_ipc_state == IPCState::Command && (b == 0x0c || b == 0x0e))
			receive_command_bit(b);
		else if (b == 0x0e)
			send_response_bit(b);
		break;
	case INT_CTRL:
		interrupt_acknowledge(b);
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
	}

	if (_debugging) DBG_EMU("< %06x %02x", _acc, b);

	return b;
}

extern bool debug_cpu;

void IO::receive_command_bit(uint8_t b) {

	DBG_EMU("cmd bit %d: %02x", _cmd_bits, b);
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
	DBG_EMU("rsp %d %02x", bit, _rsp_bit);
	_comdata = bit? 0x80: 0x00;
	_rsp_bit >>= 1;
	if (_rsp_bit == 0)
		_ipc_state = IPCState::Command;
}

void IO::link_command(uint8_t cmd) {

	DBG_EMU("link_command %02x", cmd);

	_ipc_state = IPCState::Response;

	switch (cmd) {
	case 1:
		// response is 8 bits
		_rsp_bit = 0x0080;
		_rsp = 0x40 | _kbd.available();
		debug_cpu = _kbd.available();
		break;
	case 8:
		if (_kbd.available()) {
			// response is 16 bits
			_rsp_bit = 0x8000;
			_rsp = (_kbd.available() << 12) | (_kbd.modifiers() << 8) | _kbd.read();
		} else {
			// response is 4 bits
			_rsp_bit = 0x0008;
			_rsp = 0;
		}
		break;
	}
}

void IO::poll() {

	if (_kbd.available())
		_irq_status |= INT_IPC;

	_cpu.set_interrupt_level(_irq_status? 2: 0);

//	if (_irq_status)
//		_debugging = true;
}

void IO::interrupt_acknowledge(uint8_t b) {

	if (b & INT_IPC)
		_irq_status &= ~INT_IPC;
}

void IO::begin() {
//	_machine->interval_timer(2000000, [this]() { _tick = true; });
}

void IO::checkpoint(Checkpoint &c) {
	// FIXME
}

void IO::restore(Checkpoint &c) {
	// FIXME
}
