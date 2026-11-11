#include <cstdint>

#include <pwm_sound.h>              // sound_sink

#include "snd_adapter.h"

// SoundAdapter: plays the QL's IPC sound process through a sound_sink.
//
// The sound itself is modelled by BeepModel (beep_model.cpp), which reproduces the IPC firmware's
// per-half-cycle arithmetic and was checked against golden vectors taken from the real firmware
// (ipc8049_sound_findings.md). What is still NOT modelled or not known:
//
//  - random and fuzzy: ignored. Fuzzy jitters the pitch on every half-cycle, which a 1 ms poll
//    cannot express; random perturbs the step and needs the firmware's generator.
//  - the first half-cycle starts when play() is called; the real IPC starts within about
//    11 ms of the command (when its idle timer next expires).
//  - the sink is a plain 50% square wave; amplitude and the real waveform are not modelled.
//  - timings assume an 11 MHz IPC crystal and the cost of each firmware path as measured by
//    running the listing (version unstated; 0.7 and 1.2 differ only in keyboard debounce).
//  - BEEPING reads the sound-active status bit, which this provides exactly; QDOS may cache the
//    status, so BEEPING right after a kill can read stale.

void SoundAdapter::play(const Params &p) {

	_model.start(p);                                    // replaces any sound already playing
	_start = _clock();
	_playing = true;
	_hz = 0;
	apply();
}

void SoundAdapter::stop() {

	if (_playing) {
		_model.stop();
		_sound.off();
		_playing = false;
		_hz = 0;
	}
}

void SoundAdapter::poll() {

	advance();
}

bool SoundAdapter::active() {

	advance();
	return _playing;
}

void SoundAdapter::advance() {

	if (!_playing)
		return;

	const uint64_t now = BeepModel::us_to_cycles(_clock() - _start);
	while (_model.running() && _model.ends() <= now)
		_model.next();
	apply();
}

void SoundAdapter::apply() {

	if (!_model.running()) {                            // duration expired
		_sound.off();
		_playing = false;
		_hz = 0;
		return;
	}

	const unsigned hz = _model.hz();
	if (hz == _hz)
		return;

	const uint32_t now = _clock();
	if (_hz != 0 && now - _last_tune < MIN_RETUNE_US)   // too soon: keep the old tone, catch up at a later poll
		return;

	_sound.tone(hz);
	_hz = hz;
	_last_tune = now;
}
