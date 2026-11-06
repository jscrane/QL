#include <cstdint>

#include <machine.h>
#include <memory.h>
#include <debugging.h>

#include "kbd_adapter.h"
#include "microdrive.h"
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
		if (_ipc_state == Start) {
			if (b == 0x01)
				state(Command);
		} else if (_ipc_state == Command)
			receive_command_bit(b);
		else if (_ipc_state == Parameters)
			receive_parameter_bit(b);
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
		DBG_EMU("< IO: unimplemented %06x", _acc);
		break;
	}

	if (debug_io) DBG_EMU("< IO: %06x %02x", _acc, b);

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

void IO::receive_parameter_bit(uint8_t b) {
	_params = (_params << 1) + (b == 0x0e);
	if (--_param_bits == 0) {
		state(Command);
		(this->*_cmd_handler)(_params);
	}
}

void IO::expect_parameters(uint8_t nbits, void (IO::*handler)(uint64_t)) {

	if (nbits == 0) {
		state(Command);
		(this->*handler)(0);
		return;
	}

	_param_bits = nbits;
	_params = 0;
	_cmd_handler = handler;
	state(Parameters);
}

void IO::send_response_bit(uint8_t b) {

	bool bit = _rsp & _rsp_bit;
	if (debug_io) DBG_EMU("rsp %d %02x", bit, _rsp_bit);
	_comdata = bit? 0x80: 0x00;
	_rsp_bit >>= 1;
	if (_rsp_bit == 0)
		state(Command);
}

void IO::begin_response(uint8_t nbits, uint16_t rsp) {

	--nbits;
	_rsp_bit = (1 << nbits);
	_rsp = rsp;
	state(Response);
}

void IO::link_command(uint8_t cmd) {

	if (debug_io) DBG_EMU("link_command %02x", cmd);

	switch (cmd) {
	case STATUS:
		expect_parameters(0, &IO::status);
		break;
	case READ_KEYBOARD:
		expect_parameters(0, &IO::read_keyboard);
		break;
	case READ_KEYBOARD_ROW:
		expect_parameters(4, &IO::read_kbd_row);
		break;
	case INIT_SOUND:
		expect_parameters(64, &IO::init_sound);
		break;
	case KILL_SOUND:
		expect_parameters(0, &IO::kill_sound);
		break;
	default:
		DBG_EMU("unimplemented link_command %02x", cmd);
		break;
	}
}

void IO::status(uint64_t) {
	uint16_t rsp = 0x40 | _kbd.has_key_code();
	begin_response(8, rsp);
}

void IO::read_keyboard(uint64_t) {

	uint16_t rsp = 0;
	uint8_t nbits = 4;

	if (_kbd.has_key_code()) {
		rsp = (1 << 12) | _kbd.key_code();
		nbits = 16;
	}
	begin_response(nbits, rsp);
}

void IO::read_kbd_row(uint64_t params) {
	uint8_t row = (params & 0x0f);
	DBG_EMU("read_kbd_row: %01x", row);
	// FIXME
	uint8_t rsp = 0;
	begin_response(8, rsp);
}

void IO::init_sound(uint64_t params) {
	DBG_EMU("init_sound: %lx", params);
	// FIXME
}

void IO::kill_sound(uint64_t) {
	DBG_EMU("kill_sound");
	// FIXME
}

void IO::poll() {

	if (_kbd.has_key_code())
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
	_kbd.reset();

	_cmd = _cmd_bits = _comdata = _param_bits = 0;
	_params = 0;
	_rsp = _rsp_bit = 0;
	_irq_status = 0;
	_irq_mask = 0;
	_rtc = 0;
	state(Start);
}

void IO::checkpoint(Checkpoint &c) {
	// FIXME
}

void IO::restore(Checkpoint &c) {
	// FIXME
}
