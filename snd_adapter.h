#pragma once

class sound_sink;

class SoundAdapter {
public:
	SoundAdapter(sound_sink &sound): _sound(sound) {}

	void play(/* FIXME */);
	void stop();
private:
	sound_sink &_sound;
};
