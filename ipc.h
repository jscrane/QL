#pragma once

#include <cstdint>

class KeyboardAdapter;
class SoundAdapter;

// bits in the reply to the STATUS command
enum StatusBit : uint8_t {
	STATUS_KEY               = 0x01,	// a key is pressed (we report: a key code is waiting)
	STATUS_SOUND             = 0x02,	// sound is being produced (what BEEPING reads)
	STATUS_SER1_RX           = 0x10,	// a byte has been read from serial port 1 (not emulated)
	STATUS_SER2_RX           = 0x20,	// the same for port 2 (not emulated)
	STATUS_MDV_WRITE_PROTECT = 0x40,	// cartridge write-protected (also reported when no cartridge
						// is present). Always set: with no microdrive emulated, this
						// stops QDOS trying to boot from it.
};

class IPC {
public:
	IPC(KeyboardAdapter &kbd, SoundAdapter &snd): _kbd(kbd), _snd(snd) {}
	void reset();

	void write_link(uint8_t);
	uint8_t comdata() const { return _comdata; }
	bool pending() const;

private:
	void link_command(uint8_t);
	void receive_command_bit(uint8_t);
	void receive_parameter_bit(uint8_t);
	void expect_parameters(uint8_t, void (IPC::*)(uint64_t));
	void begin_response(uint8_t, uint16_t);
	void send_next_response_bit();

	KeyboardAdapter &_kbd;
	SoundAdapter &_snd;

	enum IPCState {
		Start,
		Command,
		Parameters,
		Response
	} _ipc_state = Start;

	inline void state(IPCState s) { _ipc_state = s; }

	enum IPCCommand {
		STATUS = 1,
		READ_KEYBOARD = 8,
		READ_KEYBOARD_ROW = 9,
		INIT_SOUND = 10,
		KILL_SOUND = 11
	};

	// handlers
	void (IPC::*_cmd_handler)(uint64_t) = nullptr;
	void status(uint64_t);
	void read_keyboard(uint64_t);
	void read_kbd_row(uint64_t);
	void init_sound(uint64_t);
	void kill_sound(uint64_t);

	uint8_t _cmd = 0, _cmd_bits = 0;
	uint8_t _param_bits = 0;
	uint64_t _params = 0;
	uint8_t _comdata = 0;
	uint16_t _rsp = 0, _rsp_bit = 0;
};
