#include <cstdint>
#include <HardwareSerial.h>

#include "keyboard.h"

bool Keyboard::available() {
	while (_serial.available() && _tail < sizeof(_buf)) {
		uint8_t key = _serial.read();
		// FIXME: this is F1
		_buf[_tail++] = 0x39;
		//_buf[_tail++] = 0xb9;
	}
	return _tail > _head;
}

uint8_t Keyboard::read() {
	uint8_t key = _buf[_head++];
	if (_head == _tail)
		_head = _tail = 0;
	return key;
}

uint8_t Keyboard::modifiers() {
	return 0x00;	// FIXME
}

