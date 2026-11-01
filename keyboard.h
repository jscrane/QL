#pragma once

#include <serial_kbd.h>

class Keyboard {
public:
	Keyboard(serial_kbd &kbd): _kbd(kbd) {}

	void reset() { _kbd.reset(); }

	bool available();

	uint16_t read();

private:
	serial_kbd &_kbd;

	bool _avail;
	uint16_t _code;
};
