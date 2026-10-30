#pragma once

class serial_kbd;

class Keyboard {
public:
	Keyboard(serial_kbd &serial): _serial(serial) {}

	bool available();

	uint16_t read();

private:
	serial_kbd &_serial;

	bool _avail;
	uint16_t _code;
};
