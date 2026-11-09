#include <cstdint>
#include <cinttypes>

#include <machine.h>
#include <memory.h>
#include <debugging.h>

#include "kbd_adapter.h"
#include "snd_adapter.h"
#include "ipc.h"

inline bool data_bit(uint8_t b) { return b & 0x02; }

inline bool start_bit(uint8_t b) { return b == 0x01; }

inline bool next_response_bit(uint8_t b) { return b == 0x0e; }

void IPC::write_link(uint8_t b) {

	switch (_ipc_state) {
	case Start:
		if (start_bit(b))
			state(Command);
		break;
	case Command:
		receive_command_bit(data_bit(b));
		break;
	case Parameters:
		receive_parameter_bit(data_bit(b));
		break;
	case Response:
		if (next_response_bit(b))
			send_next_response_bit();
		break;
	}
}

void IPC::receive_command_bit(bool bit) {

	_cmd = (_cmd << 1) + bit;
	if (++_cmd_bits == 4) {
		uint8_t cmd = _cmd;
		_cmd = 0;
		_cmd_bits = 0;
		link_command(cmd);
	}
}

void IPC::receive_parameter_bit(bool bit) {

	_params = (_params << 1) + bit;
	if (--_param_bits == 0) {
		state(Command);
		(this->*_cmd_handler)(_params);
	}
}

void IPC::expect_parameters(uint8_t nbits, void (IPC::*handler)(uint64_t)) {

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

void IPC::send_next_response_bit() {

	_comdata = (_rsp & _rsp_bit);
	_rsp_bit >>= 1;
	if (_rsp_bit == 0)
		state(Command);
}

void IPC::begin_response(uint8_t nbits, uint16_t rsp) {

	--nbits;
	_rsp_bit = (1 << nbits);
	_rsp = rsp;
	state(Response);
}

void IPC::link_command(uint8_t cmd) {

	switch (cmd) {
	case STATUS:
		expect_parameters(0, &IPC::status);
		break;
	case READ_KEYBOARD:
		expect_parameters(0, &IPC::read_keyboard);
		break;
	case READ_KEYBOARD_ROW:
		expect_parameters(4, &IPC::read_kbd_row);
		break;
	case INIT_SOUND:
		expect_parameters(64, &IPC::init_sound);
		break;
	case KILL_SOUND:
		expect_parameters(0, &IPC::kill_sound);
		break;
	default:
		DBG_EMU("unimplemented ipc command %02x", cmd);
		break;
	}
}

void IPC::status(uint64_t) {

	uint16_t rsp = 0x40;		// not-connected but always set (from 8049 listing)
	if (_kbd.has_key_code()) rsp |= STATUS_KEY;
	if (_snd.active()) rsp |= STATUS_SOUND;
	begin_response(8, rsp);
}

void IPC::read_keyboard(uint64_t) {

	uint16_t rsp = 0;
	uint8_t nbits = 4;

	if (_kbd.has_key_code()) {
		rsp = (1 << 12) | _kbd.key_code();
		nbits = 16;
	}
	begin_response(nbits, rsp);
}

void IPC::read_kbd_row(uint64_t params) {
	uint8_t row = (params & 0x0f);
	DBG_EMU("read_kbd_row: %01x", row);
	// FIXME
	uint8_t rsp = 0;
	begin_response(8, rsp);
}

void IPC::init_sound(uint64_t p) {

	DBG_EMU("init_sound: 0x%" PRIx64, p);

	SoundAdapter::Params params;
	params.pitch1 = (p >> 56) & 0xff;
	params.pitch2 = (p >> 48) & 0xff;
	params.interval = __builtin_bswap16((p >> 32) & 0xffff);
	params.duration = __builtin_bswap16((p >> 16) & 0xffff);
	params.step = (p >> 12) & 0xf;
	params.wrap = (p >> 8) & 0xf;
	params.random = (p >> 4) & 0xf;
	params.fuzzy = p & 0xf;

	DBG_EMU("init_sound: pitch1=%02x pitch2=%02x interval=%04x duration=%04x step=%02x wrap=%02x random=%02x fuzzy=%02x", 
			params.pitch1, params.pitch2, params.interval, params.duration, params.step, params.wrap, params.random, params.fuzzy);
	_snd.play(params);
}

void IPC::kill_sound(uint64_t) {
	DBG_EMU("kill_sound");
	_snd.stop();
}

bool IPC::pending() const { return _kbd.has_key_code(); }

void IPC::reset() {

	_kbd.reset();
	_snd.stop();

	_cmd_handler = nullptr;
	_cmd = _cmd_bits = _param_bits = 0;
	_comdata = false;
	_params = 0;
	_rsp = _rsp_bit = 0;
	state(Start);
}
