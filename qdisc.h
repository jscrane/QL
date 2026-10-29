#pragma once

#include "wd1770.h"

class QDisc: public Memory::Device {
public:
	QDisc(flash_file &image): Memory::Device(256), _wd(image) {}
	void reset() { _wd.reset(); }

	void operator=(uint8_t) override;
	operator uint8_t() override;

	void checkpoint(Checkpoint &) override {}
	void restore(Checkpoint &) override {}

private:
	void write_latch(uint8_t);

	wd1770 _wd;
	uint8_t _latch = 0;
};
