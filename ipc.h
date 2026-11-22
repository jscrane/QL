#pragma once

#include <cstdint>

class KeyboardAdapter;
class SoundAdapter;
class Checkpoint;

// bits in the reply to the STATUS command
enum StatusBit : uint8_t {
	STATUS_KEY               = 0x01,	// a key is pressed (we report: a key code is waiting)
	STATUS_SOUND             = 0x02,	// sound is being produced (what BEEPING reads)
	STATUS_SER1_RX           = 0x10,	// a byte has been read from serial port 1 (not emulated)
	STATUS_SER2_RX           = 0x20,	// the same for port 2 (not emulated)
};

class IPC {
public:
	IPC(KeyboardAdapter &kbd, SoundAdapter &snd): _kbd(kbd), _snd(snd) {}
	void reset();

	void write_link(uint8_t);
	bool comdata() const { return _comdata; }
	bool pending() const;

	void checkpoint(Checkpoint &);
	void restore(Checkpoint &);
private:
	void link_command(uint8_t);
	void receive_command_bit(bool);
	void receive_parameter_bit(bool);
	void begin_response(uint8_t, uint16_t);
	void send_next_response_bit();

	KeyboardAdapter &_kbd;
	SoundAdapter &_snd;

	enum IPCState: uint8_t {
		Start,
		Command,
		Parameters,
		Response
	} _ipc_state = Start;

	inline void state(IPCState s) { _ipc_state = s; }

	enum IPCCommand: uint8_t {
		STATUS = 1,
		READ_KEYBOARD = 8,
		READ_KEYBOARD_ROW = 9,
		INIT_SOUND = 10,
		KILL_SOUND = 11,
		SET_MDRS = 12,
		SET_BAUD_RATE = 13,
		RANDOM = 14,
		TEST = 15,
	};

	// handlers
	void status(uint64_t);
	void read_keyboard(uint64_t);
	void read_kbd_row(uint64_t);
	void init_sound(uint64_t);
	void kill_sound(uint64_t);
	void mdrs(uint64_t);
	void set_baud_rate(uint64_t);
	void random(uint64_t);
	void test(uint64_t);

	struct command {
		uint8_t cmd;
		uint8_t nbits;
		void (IPC::*handler)(uint64_t);
	};
	static const command *find_command(uint8_t);
	void expect_parameters(const command &);

	uint8_t _cmd = 0, _cmd_bits = 0;
	uint8_t _param_cmd = 0;
	uint8_t _param_bits = 0;
	uint64_t _params = 0;
	bool _comdata = false;
	uint16_t _rsp = 0, _rsp_bit = 0;
};
