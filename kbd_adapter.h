#pragma once

#include <keyboard.h>

class KeyboardAdapter {
public:
	KeyboardAdapter(keyboard &kbd): _kbd(kbd) {}

	void reset() { _kbd.reset(); }

	bool available();

	uint16_t read();

private:
	keyboard &_kbd;

	bool _avail;
	uint16_t _code;
};
