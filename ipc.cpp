#include <cstdint>
#include <cinttypes>

#include <machine.h>
#include <memory.h>
#include <debugging.h>

#include "kbd_adapter.h"
#include "snd_adapter.h"
#include "ipc.h"

// see here: https://github.com/zechmer/ipc8049

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
		const command *c = find_command(_param_cmd);
		if (c)
			(this->*c->handler)(_params);
	}
}

void IPC::expect_parameters(const struct command &c) {

	if (c.nbits == 0) {
		state(Command);
		(this->*c.handler)(0);
		return;
	}

	_param_cmd = c.cmd;
	_param_bits = c.nbits;
	_params = 0;
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

const IPC::command *IPC::find_command(uint8_t cmd) {

	static const command commands[] = {
		{ STATUS,		0,	&IPC::status },
		{ READ_KEYBOARD,	0,	&IPC::read_keyboard },
		{ READ_KEYBOARD_ROW,	4,	&IPC::read_kbd_row },
		{ INIT_SOUND,		64,	&IPC::init_sound },
		{ KILL_SOUND,		0,	&IPC::kill_sound },
		{ SET_MDRS,		4,	&IPC::mdrs },
		{ SET_BAUD_RATE,	4,	&IPC::set_baud_rate },
		{ RANDOM,		0,	&IPC::random },
		{ TEST,			8,	&IPC::test },
	};
	for (const auto &c: commands)
		if (c.cmd == cmd)
			return &c;
	return nullptr;
}

void IPC::link_command(uint8_t cmd) {

	const struct command *c = find_command(cmd);
	if (c)
		expect_parameters(*c);
	else
		DBG_EMU("unimplemented ipc command %02x", cmd);
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

void IPC::mdrs(uint64_t params) {
	uint8_t data = (params & 0x0f);
	DBG_EMU("mdrs: %01x", data);
}

void IPC::set_baud_rate(uint64_t params) {
	uint8_t baud = (params & 0x0f);
	DBG_EMU("set_baud_rate: %01x", baud);
}

void IPC::random(uint64_t) {
	DBG_EMU("random: FIXME");
	begin_response(16, 0xffff);
}

void IPC::test(uint64_t params) {
	uint8_t t = (params & 0xff);
	DBG_EMU("test: %02x", t);
	begin_response(8, t);
}

bool IPC::pending() const { return _kbd.has_key_code(); }

void IPC::reset() {

	_kbd.reset();
	_snd.stop();

	_cmd = _cmd_bits = _param_cmd = _param_bits = 0;
	_comdata = false;
	_params = 0;
	_rsp = _rsp_bit = 0;
	state(Start);
}

void IPC::checkpoint(Checkpoint &c) {

	c.write(_cmd);
	c.write(_cmd_bits);
	c.write(_param_cmd);
	c.write(_param_bits);
	c.write(_comdata);
	c.write(_params);
	c.write(_rsp);
	c.write(_rsp_bit);
	c.write((uint8_t)_ipc_state);
}

void IPC::restore(Checkpoint &c) {

	c.read(_cmd);
	c.read(_cmd_bits);
	c.read(_param_cmd);
	c.read(_param_bits);
	c.read(_comdata);
	c.read(_params);
	c.read(_rsp);
	c.read(_rsp_bit);
	c.read((uint8_t&)_ipc_state);
	_kbd.reset();
	_snd.stop();
}
