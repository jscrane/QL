#pragma once

#include <serial_kbd.h>

class fnkey_kbd: public serial_kbd {
public:
	fnkey_kbd(serial_kbd &src): _src(src) {}

	bool available() override {
		while (!_have && _src.available()) {
			int c = _src.read();
			if (c >= 0 && !claimed(c)) {
				_next = c;
				_have = true;
			}
		}
		return _have;
	}

	int read() override {
		if (!available())
			return -1;
		_have = false;
		return _next;
	}

	void reset() override { _src.reset(); _have = false; }

private:
	bool claimed(int c) {
		if (c & ~KEY_MASK)			// modified: leave for the guest
			return false;
		return c >= KEY_F1 && c <= KEY_F12 && handle_fnkey(c - KEY_F1 + 1);
	}

	serial_kbd &_src;
	int _next = -1;
	bool _have = false;
};
