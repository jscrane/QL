#pragma once

#include <fnkey_kbd.h>

class KeyboardAdapter: private fnkey_kbd {
public:
	KeyboardAdapter(keyboard &kbd): fnkey_kbd(kbd) {}

	using fnkey_kbd::reset;

	using fnkey_kbd::register_fnkey_handler;

	bool has_key_code();

	uint16_t key_code();

private:
	bool _avail;
	uint16_t _code;
};
