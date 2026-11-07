#include <pwm_sound.h>

#include "snd_adapter.h"

void SoundAdapter::stop() {
	_sound.off();
}

void SoundAdapter::play() {
	// FIXME
}
