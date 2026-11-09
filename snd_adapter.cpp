#include <cstdint>

#include <debugging.h>
#include <pwm_sound.h>              // sound_sink

#include "snd_adapter.h"

// SoundAdapter: emulates the QL's IPC sound process on top of a sound_sink.
//
// Things we are NOT sure about (tags: [observed] in wire traces and tests against QDOS JS,
// [documented] in the QL manual, [assumed] by us). Revisit as each is settled.
//
// Pitch
//  - pitch_to_hz() uses an empirical fit (f ~= 11336.256 / (N + 7.634), N = IPC pitch byte,
//    0 treated as 256), taken from the QBITS article. It is a fit, not derived from the
//    8049 firmware; the article reports up to about 1 Hz deviation mid-range.
//  - [observed] the IPC pitch byte is the BEEP pitch + 1, wrapping at 8 bits (BEEP 255 -> 0).
//    We assume this means a down-counter where 0 = 256, but have not confirmed it.
//
// Duration
//  - [documented] BEEP duration is signed (-32768..32767), units of 72 us, 0 = until stopped.
//    We treat the 16 wire bits as UNSIGNED, so -1 plays for ~4.7 s ([observed] ~5 s).
//    The QBITS article claims 2.36 s for -1 (as if bit 15 were ignored). Unresolved.
//  - One enthusiast article reports that duration 0 distorts the real pitch. Not modelled.
//  - Durations are rounded up to whole ms for the sink; a 72 us click becomes 1 ms.
//
// Sweep (pitch2, interval, step, wrap) -- NOT IMPLEMENTED YET, see pitch_at()
//  - [documented] sound bounces between pitch and pitch2, one step of `step` every `interval`.
//  - [assumed] positive step = pitch number rises (lower tone); first step lands at t = interval.
//  - [documented, community source] pitch2 with the same or a higher tone than pitch1
//    (numerically <= pitch1) has no effect.
//  - Unknown: negative step behaviour; what happens at the turning point (dwell or not);
//    overshoot handling (clamp, skip, or reverse early).
//  - Unknown: wrap. One manual copy says 15 = forever; another says the range repeats
//    N times before the direction changes.
//  - [assumed] the 16-bit interval is low byte first, like duration.
//
// Randomness and fuzziness
//  - [documented] wire layout puts randomness in the top nibble and fuzziness in the low
//    nibble; the BEEP argument order is fuzzy then random. Algorithms unknown.
//
// Output
//  - We emit a plain 50% square wave via sound_sink::tone(); amplitude and the real IPC
//    waveform are not modelled.
//  - Real-hardware side effects are not modelled (e.g. very high pitches hampering the
//    IPC's keyboard handling).
//
// Behaviour of the surrounding system
//  - [assumed] a new initiate-sound replaces any sound already playing; kill stops all.
//  - BEEPING returns the sound-active status bit, which we compute lazily from the clock.
//    QDOS may cache the status, so BEEPING right after a kill can read stale (see the
//    `BEEP: PRINT BEEPING` observation).

unsigned SoundAdapter::pitch_to_hz(uint8_t ipc_pitch) {
	unsigned n = ipc_pitch ? ipc_pitch : 256;          // 0 wraps to 256
	return (unsigned)((11336256 + 500) / (1000*n + 7634));
}

uint8_t SoundAdapter::pitch_at(const Params &p, uint32_t t) {
	return p.pitch1;
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
