#pragma once

#include <serial_kbd.h>

class Keyboard {
public:
	Keyboard(serial_kbd &serial): _serial(serial) {}

	void reset() { _serial.reset(); }

	bool available();

	uint16_t read();

private:
	serial_kbd &_serial;

	bool _avail;
	uint16_t _code;
};
