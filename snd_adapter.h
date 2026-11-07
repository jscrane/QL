#pragma once

#include <cstdint>
#include <functional>

class sound_sink;

class SoundAdapter {
public:
	struct Params {
		uint8_t  pitch1, pitch2;
		uint16_t interval, duration;    // 72 us units
		int8_t   step;                  // signed 4-bit: -8..7
		uint8_t  wrap, random, fuzzy;
	};
	using Clock = std::function<uint32_t()>;    // microseconds

	SoundAdapter(sound_sink &sound, Clock clock): _sound(sound), _clock(clock) {}

	void play(const Params &);
	void stop();
	bool active() const;

	static unsigned pitch_to_hz(uint8_t ipc_pitch);

private:
	static constexpr uint32_t UNIT_US = 72;

	sound_sink &_sound;
	Clock _clock;
	bool _playing = false;
	uint32_t _start = 0, _duration_us = 0;      // 0 = until stopped
};
