#pragma once

class HardwareSerial;

class Keyboard {
public:
	Keyboard(HardwareSerial &serial): _serial(serial) {}

	bool available();

	uint8_t read();

	uint8_t modifiers();

private:
	HardwareSerial &_serial;

	uint8_t _buf[16], _head = 0, _tail = 0;
};
