#include "beep_model.h"

namespace {

// Machine cycles spent in the firmware's timer interrupt, measured on the real IPC firmware.
constexpr uint32_t TICK = 32;           // machine cycles per timer tick
constexpr uint32_t PATH_BASE = 130;     // no step due, no duration
constexpr uint32_t PATH_DURATION = 55;  // extra when duration != 0
constexpr uint32_t PATH_STEP = 96;      // extra when a step is taken and the pitch stays in range
constexpr uint32_t PATH_REVERSE = 118;  // extra when the step leaves the range and the direction reverses
constexpr uint32_t PATH_WRAP = 122;     // extra when it leaves the range and the sweep restarts instead
constexpr uint32_t PATH_EXPIRE = 205;   // total, when the duration expires

unsigned halfcycle_hz(uint32_t cycles) {
	return (366667 + cycles / 2) / cycles;      // 11 MHz / 15 cycles per us / (2 half-cycles)
}

}

BeepModel::State BeepModel::following(const State &s) const {

	State n = s;
	const bool timed = _p.duration != 0;
	const uint32_t count = s.pitch ? s.pitch : 256;
	const uint16_t acc = s.pitch ? s.pitch : 255;       // the firmware adds 255 when the count is 0
	uint32_t cycles = count * TICK + PATH_BASE + (timed ? PATH_DURATION : 0);

	if (timed) {
		n.dur_acc = uint16_t(s.dur_acc + acc + 6);
		if (n.dur_acc >= _p.duration) {                 // unsigned; the accumulator can wrap past it
			n.stop = true;
			n.t = s.t + count * TICK + PATH_EXPIRE;
			return n;
		}
	}

	n.int_acc = uint16_t(s.int_acc + acc + (timed ? 6 : 4));
	if (n.int_acc >= _p.interval) {                     // always true when interval is 0
		n.int_acc = 0;
		uint8_t px = uint8_t(s.pitch + s.step);
		if (px > _p.pitch1 && px < _p.pitch2) {
			cycles += PATH_STEP;
		} else if (_p.wrap == 15 || (_p.wrap != 0 && s.wrap_count < _p.wrap)) {
			n.wrap_count = uint8_t(s.wrap_count + 1);
			px = s.step >= 0 ? _p.pitch1 : _p.pitch2;   // restart: the out-of-range value is never played
			cycles += PATH_WRAP;
		} else {
			n.wrap_count = 0;
			n.step = s.step == -8 ? -8 : -s.step;       // 4-bit negate: -8 stays -8
			cycles += PATH_REVERSE;                     // the out-of-range value is played once
		}
		n.pitch = px;
	}
	n.t = s.t + cycles;
	return n;
}

void BeepModel::start(const BeepParams &p) {

	_p = p;
	_cur = State{ 0, uint8_t(p.step >= 0 ? p.pitch1 : p.pitch2), p.step, 0, 0, 0, false };
	_nxt = following(_cur);
	_running = true;
}

void BeepModel::next() {

	if (!_running)
		return;
	_cur = _nxt;
	if (_cur.stop)
		_running = false;
	else
		_nxt = following(_cur);
}

unsigned BeepModel::hz() const {

	const uint32_t count = _cur.pitch ? _cur.pitch : 256;
	uint32_t path = PATH_BASE + (_p.duration ? PATH_DURATION : 0);
	if (_p.interval == 0)                               // the step code runs in every interrupt
		path += _p.step == 0 ? PATH_REVERSE : PATH_STEP;
	return halfcycle_hz(count * TICK + path);
}

unsigned BeepModel::steady_hz(uint8_t pitch, bool timed) {

	const uint32_t count = pitch ? pitch : 256;
	return halfcycle_hz(count * TICK + PATH_BASE + PATH_REVERSE + (timed ? PATH_DURATION : 0));
}
