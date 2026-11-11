#include <cstdint>
#include <vector>

#include <keyboard.h>
#include <check.h>

#include "trace_sound.h"
#include "fake_clock.h"
#include "snd_adapter.h"
#include "kbd_adapter.h"
#include "ipc.h"

// a keyboard that returns a scripted sequence of characters
class scripted_kbd: public keyboard {
public:
	void push(int c) { _q.push_back(c); }
	bool available() override { return !_q.empty(); }
	int read() override {
		if (_q.empty()) return -1;
		int c = _q.front();
		_q.erase(_q.begin());
		return c;
	}
	void reset() override { _q.clear(); }
private:
	std::vector<int> _q;
};

// everything the IPC needs, wired to fakes. Declaration order = construction order.
struct Rig {
	scripted_kbd serial;
	KeyboardAdapter kbd;
	trace_sound trace;
	SoundAdapter snd;
	IPC ipc;

	Rig(): kbd(serial), trace(clk), snd(trace, clk), ipc(kbd, snd) {   // adjust to your constructor
		now = 0;
		ipc.reset();
		ipc.write_link(0x01);                  // start bit, as after reset
	}
};

// MSB first; the link encodes a 1 as 0x0e and a 0 as anything else
static void send_bits(IPC &ipc, uint64_t v, int n) {
	for (int i = n - 1; i >= 0; --i)
		ipc.write_link(((v >> i) & 1) ? 0x0e : 0x00);
}

static void send_command(IPC &ipc, uint8_t cmd) { send_bits(ipc, cmd, 4); }

// each 0x0e write clocks out the next reply bit, MSB first, readable via comdata()
static unsigned read_response(IPC &ipc, int nbits) {
	unsigned v = 0;
	for (int i = 0; i < nbits; ++i) {
		ipc.write_link(0x0e);
		v = (v << 1) | ipc.comdata();
	}
	return v;
}

enum { STATUS = 0x1, READ_KEYBOARD = 0x8, READ_KBD_ROW = 0x9, INIT_SOUND = 0xa, KILL_SOUND = 0xb };

// a command with a short, predictable reply, used to prove the link is still in sync
static void check_in_sync(Rig &r) {
	send_command(r.ipc, READ_KEYBOARD);
	CHECK_EQ(read_response(r.ipc, 4), 0);
}

static bool tone_started(const trace_sound &t) {
	for (const auto &e : t.events())
		if (e.hz != 0) return true;
	return false;
}

// ---- keyboard / status ----

static void test_status_no_key() {
	Rig r;
	send_command(r.ipc, STATUS);
	CHECK_EQ(read_response(r.ipc, 8), 0x40);
}

static void test_commands_back_to_back() {
	// documents the current framing: no new start bit between commands
	Rig r;
	send_command(r.ipc, STATUS);
	CHECK_EQ(read_response(r.ipc, 8), 0x40);
	send_command(r.ipc, STATUS);
	CHECK_EQ(read_response(r.ipc, 8), 0x40);
}

static void test_read_keyboard_no_key() {
	Rig r;
	send_command(r.ipc, READ_KEYBOARD);
	CHECK_EQ(read_response(r.ipc, 4), 0);
}

static void test_read_keyboard_with_key() {
	// only checks framing, not the key mapping: bit 12 set, nothing above it
	Rig r;
	r.serial.push('a');
	CHECK(r.ipc.pending());
	send_command(r.ipc, STATUS);
	CHECK_EQ(read_response(r.ipc, 8), 0x41);
	send_command(r.ipc, READ_KEYBOARD);
	unsigned rsp = read_response(r.ipc, 16);
	CHECK_EQ(rsp >> 12, 1);
	CHECK(!r.ipc.pending());
}

static void test_kbd_row_framing() {
	// 4 parameter bits, 8 reply bits; the value is 0 until read_kbd_row is implemented
	Rig r;
	send_command(r.ipc, READ_KBD_ROW);
	send_bits(r.ipc, 3, 4);
	CHECK_EQ(read_response(r.ipc, 8), 0);
	check_in_sync(r);
}

static void test_unknown_command_keeps_sync() {
	Rig r;
	send_command(r.ipc, 0x2);                  // not implemented
	check_in_sync(r);
}

// ---- sound ----
static void test_init_sound_beep_1000_10() {
	Rig r;
	send_command(r.ipc, INIT_SOUND);
	send_bits(r.ipc, 0x0b0b0000e8030000ull, 64);   // captured from BEEP 1000,10 on QDOS JS
	CHECK_EQ(r.trace.events().size(), 1);
	CHECK_EQ(r.trace.events()[0].hz, 560);          // a timed tone sounds lower than BEEP 0,10 (611 Hz)
	CHECK(r.snd.active());
	check_in_sync(r);                               // exactly 64 parameter bits consumed
	now += 50000;  CHECK(r.snd.active());           // the firmware's accumulator ends it at about 52.6 ms
	now += 5000;   CHECK(!r.snd.active());
}

static void test_init_sound_other_pitches() {
	{
		Rig r;                                      // BEEP 258,0: IPC pitch 1, duration 0x0102
		send_command(r.ipc, INIT_SOUND);
		send_bits(r.ipc, 0x0101000002010000ull, 64);
		CHECK_EQ(r.trace.events()[0].hz, 1095);
		now += 15000;  CHECK(r.snd.active());       // ends at about 16.8 ms
		now += 3000;   CHECK(!r.snd.active());
	}
	{
		Rig r;                                      // BEEP 258,255: pitch wraps to 0 (= 256)
		send_command(r.ipc, INIT_SOUND);
		send_bits(r.ipc, 0x0000000002010000ull, 64);
		CHECK_EQ(r.trace.events()[0].hz, 43);
		now += 11000;  CHECK(r.snd.active());       // ends after one half-cycle, at about 11.4 ms
		now += 1000;   CHECK(!r.snd.active());
	}
}

static void test_kill_sound() {
	Rig r;
	send_command(r.ipc, INIT_SOUND);
	send_bits(r.ipc, 0x0b0b0000e8030000ull, 64);
	send_command(r.ipc, KILL_SOUND);           // no parameters, no reply
	CHECK(!r.snd.active());
	CHECK_EQ(r.trace.events().back().hz, 0);
	check_in_sync(r);
}

static void test_reset_mid_parameters() {
	Rig r;
	send_command(r.ipc, INIT_SOUND);
	send_bits(r.ipc, 0x0b, 10);                // abandon part-way through
	r.ipc.reset();
	CHECK(!tone_started(r.trace));
	r.ipc.write_link(0x01);                    // start bit
	check_in_sync(r);
}

static void test_reset_stops_sound() {
	Rig r;
	send_command(r.ipc, INIT_SOUND);
	send_bits(r.ipc, 0x0b0b0000e8030000ull, 64);
	r.ipc.reset();
	CHECK(!r.snd.active());
	CHECK_EQ(r.trace.events().back().hz, 0);
}

const uint8_t SOUND_BIT = 2;

static void test_status_reports_sound() {
	Rig r;
	send_command(r.ipc, STATUS);
	CHECK_EQ(read_response(r.ipc, 8) & SOUND_BIT, 0);

	send_command(r.ipc, INIT_SOUND);
	send_bits(r.ipc, 0x0b0b0000e8030000ull, 64);
	send_command(r.ipc, STATUS);
	CHECK_EQ(read_response(r.ipc, 8) & SOUND_BIT, SOUND_BIT);

	now += 72000;
	send_command(r.ipc, STATUS);
	CHECK_EQ(read_response(r.ipc, 8) & SOUND_BIT, 0);   // timed tone has expired
}

static void test_status_clear_after_kill() {
	Rig r;
	send_command(r.ipc, INIT_SOUND);
	send_bits(r.ipc, 0x0b0b0000e8030000ull, 64);
	send_command(r.ipc, KILL_SOUND);
	send_command(r.ipc, STATUS);
	CHECK_EQ(read_response(r.ipc, 8) & SOUND_BIT, 0);   // no gap between kill and status
}

int main() {
	RUN(test_status_no_key);
	RUN(test_commands_back_to_back);
	RUN(test_read_keyboard_no_key);
	RUN(test_read_keyboard_with_key);
	RUN(test_kbd_row_framing);
	RUN(test_unknown_command_keeps_sync);
	RUN(test_init_sound_beep_1000_10);
	RUN(test_init_sound_other_pitches);
	RUN(test_kill_sound);
	RUN(test_reset_mid_parameters);
	RUN(test_reset_stops_sound);
	RUN(test_status_reports_sound);
	RUN(test_status_clear_after_kill);
	return failures != 0;
}
