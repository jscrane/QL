#pragma once

#include <cstdint>

// What the QL's IPC (an Intel 8049) was told to do by an "initiate sound" command.
// Fields are the wire values, so `pitch1`/`pitch2` are the BEEP pitches plus one.
struct BeepParams {
	uint8_t  pitch1, pitch2;        // BEEP pitch + 1 (0 means 256)
	uint16_t interval, duration;    // grad_x and duration, as sent: unsigned 16-bit
	int8_t   step;                  // grad_y: signed 4-bit, -8..7
	uint8_t  wrap, random, fuzzy;   // random and fuzzy are not modelled
};

// The IPC's sound process, modelled one half-cycle at a time.
//
// The real firmware drives the speaker from a timer interrupt: each interrupt toggles the pin,
// bumps two accumulators (duration and step interval), possibly steps the pitch, and reloads the
// timer, so one half-cycle lasts (pitch count * 32 + the cost of that interrupt) machine cycles.
// This class repeats that arithmetic, using costs measured by running the real firmware
// (see ipc8049_sound_findings.md), so durations and step intervals come out in the firmware's own
// units rather than some fixed number of microseconds.
//
// Times are machine cycles since start() (1 cycle = 15/11 us, from an 11 MHz crystal).
class BeepModel {
public:
	void start(const BeepParams &);
	void stop() { _running = false; }
	bool running() const { return _running; }

	uint8_t pitch() const { return _cur.pitch; }        // IPC pitch byte of the current half-cycle
	uint64_t started() const { return _cur.t; }         // when it began (when the sound ended, if stopped)
	uint64_t ends() const { return _nxt.t; }            // when the next half-cycle begins
	unsigned hz() const;                                // steady frequency for the current pitch
	void next();                                        // move on to the next half-cycle (or stop)

	static unsigned steady_hz(uint8_t pitch, bool timed = false);   // BEEP duration,pitch with no sweep

	static uint64_t us_to_cycles(uint32_t us) { return uint64_t(us) * 11 / 15; }
	static uint32_t cycles_to_us(uint64_t c) { return uint32_t((c * 15 + 5) / 11); }

private:
	struct State {
		uint64_t t;                 // start of this half-cycle
		uint8_t pitch;              // timer count for this half-cycle
		int8_t step;                // current step (negated at each reversal)
		uint8_t wrap_count;
		uint16_t int_acc, dur_acc;  // the firmware's 16-bit accumulators
		bool stop;                  // the sound ends at t
	};

	State following(const State &) const;

	BeepParams _p{};
	State _cur{}, _nxt{};
	bool _running = false;
};
