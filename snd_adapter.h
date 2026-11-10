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
	using Clock = std::function<uint32_t()>;    // microseconds

	SoundAdapter(sound_sink &sound, Clock clock): _sound(sound), _clock(clock) {}

	void play(const Params &);
	void stop();
	bool active();                              // advances the model first, so it is never stale
	void poll() override;

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
};
