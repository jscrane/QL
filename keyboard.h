#pragma once

class Keyboard {
public:
	Keyboard(HardwareSerial &serial): _serial(serial) {}

	int available() {
		while (_serial.available() && _tail < sizeof(_buf)) {
			uint8_t key = _serial.read();
			/*
			_buf[_tail++] = key;
			_buf[_tail++] = key | 0x80;
			*/
			_buf[_tail++] = 0x01;
			_buf[_tail++] = 0x81;
		}
		return _tail - _head;
	}

	uint8_t read() {
		uint8_t key = _buf[_head++];
		if (_head == _tail)
			_head = _tail = 0;
		return key;
	}

	uint8_t modifiers() {
		return 0x00;	// FIXME
	}

private:
	HardwareSerial &_serial;
	uint8_t _buf[16], _head = 0, _tail = 0;
};
