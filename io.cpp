#include <cstdint>

#include <machine.h>
#include <memory.h>
#include <debugging.h>
#include <serial_kbd.h>
#include <CPU.h>
#include <m68k.h>

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
	}
}

void IO::receive_command_bit(uint8_t b) {
	_send = 0;
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
	bool bit = _rsp & (0x80 >> _rsp_bits);
	DBG("rsp %d %02x", bit, (0x80 >> _rsp_bits));
	_send = bit? 0x80: 0x00;
	if (++_rsp_bits == 8) {
		_ipc_state = IPCState::Command;
		DBG_EMU("sent %02x", _rsp);

//		extern bool debug_cpu;
//		debug_cpu = true;
	}
}

void IO::link_command(uint8_t cmd) {

	DBG_EMU("link_command %02x", cmd);

	switch (cmd) {
	case 1:
		_ipc_state = IPCState::Response;
		_rsp_bits = 0;
		_rsp = 0x40 | (_has_key? 1: 0);
		break;
	case 8:
		_ipc_state = IPCState::Response;
		_rsp_bits = 0;
		_rsp = 0x01;	// "F1 down"
		_has_key = false;
		break;
	}
}

IO::operator uint8_t() {

	uint8_t b = 0;

	switch (_acc) {
	case INT_IPC_STAT:
		b = interrupt_status();
		break;
	case MDV_RS_STAT:
		b = _send;
		break;
	}

	if (_debugging) DBG_EMU("< %06x %02x", _acc, b);

	return b;
}

void IO::poll() {

	if (!_has_key)
		_debugging = false;

	if (_kbd.available()) {
		_has_key = true;
		_key = _kbd.read();
		_cpu.set_interrupt_level(2);
		_debugging = true;
	}
}

uint8_t IO::interrupt_status() {
	uint8_t status = 0;

	if (_has_key)
		status |= 0x02;

	return status;
}

void IO::checkpoint(Checkpoint &c) {
	// FIXME
}

void IO::restore(Checkpoint &c) {
	// FIXME
}
