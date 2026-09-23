#pragma once

class HardwareSerial;

class Keyboard {
public:
	Keyboard(HardwareSerial &serial): _serial(serial) {}

	bool available();

	uint16_t read();

private:
	HardwareSerial &_serial;

	uint8_t _head = 0, _tail = 0, _avail = 0;
	uint16_t _buf[16];
};
