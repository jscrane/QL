#pragma once

class HardwareSerial;

class Keyboard {
public:
	Keyboard(HardwareSerial &serial): _serial(serial) {}

	bool available();

	uint16_t read();

private:
	HardwareSerial &_serial;

	bool _avail;
	uint16_t _code;
};
