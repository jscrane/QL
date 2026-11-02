#pragma once

#include <display.h>

class Screen: public Memory::Device {
public:
	Screen(): Memory::Device(0x10000) {}
	void begin();
	void reset();
	void set_mode(uint8_t);
	void status(const char *message) { _dsp.status(message); }

	void operator=(uint8_t) override;
	operator uint8_t() override { return _ram.get(_acc); }

	void checkpoint(Checkpoint &) override;
	void restore(Checkpoint &) override;

private:
	void draw(Memory::address);
	void draw_mode4(Memory::address, uint8_t, uint8_t);
	void draw_mode8(Memory::address, uint8_t, uint8_t);
	void redraw();
	void set_screen();

	inline Memory::address screen_base() const { return _screen2? 0x8000: 0x0000; }
	inline bool is_visible(Memory::address a) const { return _screen2? a >= 0x8000: a < 0x8000; }

	dram<0x10000> _ram;
	bool _mode4, _screen2;
	Display _dsp;
};
