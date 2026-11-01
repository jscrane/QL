#pragma once

#include <serial_kbd.h>

class fnkey_kbd: public serial_kbd {
public:
	fnkey_kbd(serial_kbd &src): _src(src) {}

	bool available() override { return _src.available(); }

	int read() override {
		int c = _src.read();
		if (c < 0 || claimed(c))
			return -1;			// nothing, or the emulator took it
		return c;
	}

	void reset() override { _src.reset(); }

private:
	bool claimed(int c) {
		if (c & ~keys::KEY_MASK)		// modified: leave for the guest
			return false;
		return c >= keys::KEY_F1 && c <= keys::KEY_F12 && handle_fnkey(c - keys::KEY_F1 + 1);
	}

	serial_kbd &_src;
};
