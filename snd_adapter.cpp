#include <cstdint>

#include <debugging.h>
#include <pwm_sound.h>              // sound_sink

#include "snd_adapter.h"

unsigned SoundAdapter::pitch_to_hz(uint8_t ipc_pitch) {
	unsigned n = ipc_pitch ? ipc_pitch : 256;          // 0 wraps to 256
	return (unsigned)(11336.256f / (n + 7.634f) + 0.5f);
}

void SoundAdapter::play(const Params &p) {
	if (p.pitch2 != p.pitch1 || p.interval || p.step || p.wrap || p.random || p.fuzzy)
		DBG_EMU("sound: sweep parameters not supported yet");

	uint32_t us = uint32_t(p.duration) * UNIT_US;      // FIXME: negative durations
	unsigned long ms = us ? (us + 999) / 1000 : 0;     // round up; 0 means until stopped

	_sound.tone(pitch_to_hz(p.pitch1), ms);            // replaces any tone already playing
	_start = _clock();
	_duration_us = us;
	_playing = true;
}

void SoundAdapter::stop() {
	if (_playing) {
		_sound.off();
		_playing = false;
	}
}

bool SoundAdapter::active() const {
	return _playing && (_duration_us == 0 || (_clock() - _start) < _duration_us);
}
