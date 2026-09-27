#pragma once

class Microdrive {
public:
	void write_control(uint8_t b) {

		// Empty MDV1 presents a gap.
		_gap = _control == 0x03 && b == 0x01;

		_control = b;
	}

	bool gap() const {
		return _gap;
	}

private:
	uint8_t _control = 0;
	bool _gap = false;
};
