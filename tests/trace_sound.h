#pragma once

#include <cstdint>
#include <vector>
#include <pwm_sound.h>

class trace_sound: public sound_sink {
public:
	// us: clock when the call was made; hz == 0 means off;
	struct event { uint32_t us; unsigned hz; };

	trace_sound(uint32_t (*clock)()): _clock(clock) {}

	void tone(unsigned hz) override { _events.push_back({ _clock(), hz }); }
	void off() override { _events.push_back({ _clock(), 0 }); }

	const std::vector<event> &events() const { return _events; }
	void clear() { _events.clear(); }

private:
	uint32_t (*_clock)();
	std::vector<event> _events;
};
