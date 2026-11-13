#pragma once

#include <cstdint>
#include <functional>
#include <machine.h>

#include "beep_model.h"

class sound_sink;

// SoundAdapter: plays the QL's IPC sound process through a sound_sink.
//
// BeepModel works out what the IPC would be doing (pitch, steps, when it stops); the adapter
// advances it to the current time whenever it is polled or asked, and retunes the sink when the
// frequency changes. Polled about every 1 ms, it can follow a step interval down to roughly
// 23 units (about 1 ms); faster sweeps come out as a coarse glide, because the model keeps
// running at full speed but the sink is only retuned once per poll.
class SoundAdapter: public Pollable {
public:
	using Params = BeepParams;

	SoundAdapter(sound_sink &sound, Clock clock): _sound(sound), _clock(clock) {}

	void play(const Params &);
	void stop();
	bool active();                              // advances the model first, so it is never stale
	void poll() override;

	// The sink is retuned at most this often. The model itself keeps running at full resolution, so a
	// sweep that steps faster than this is sampled (a coarse glide). Without a limit, a fast sweep
	// floods the sink: on an ESP32 every tone() is a message to a high-priority task that retunes the
	// LEDC hardware, and a burst starves the idle task until the task watchdog aborts.
	static constexpr uint32_t MIN_RETUNE_US = 10000;

	// frequency of BEEP duration,pitch (no sweep) for an IPC pitch byte
	static unsigned pitch_to_hz(uint8_t ipc_pitch) { return BeepModel::steady_hz(ipc_pitch); }

private:
	void advance();
	void apply();

	sound_sink &_sound;
	Clock _clock;
	BeepModel _model;
	bool _playing = false;
	uint32_t _start = 0;
	unsigned _hz = 0;
	uint32_t _last_tune = 0;
};
