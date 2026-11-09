#undef NDEBUG                      // make sure assert() is live
#include <cassert>
#include <cstdint>
#include <check.h>
#include "trace_sound.h"           // events now carry {us, hz, ms}
#include "fake_clock.h"
#include "snd_adapter.h"

static SoundAdapter::Params beep(uint8_t pitch, uint16_t duration) {
	SoundAdapter::Params p{};
	p.pitch1 = p.pitch2 = pitch;
	p.duration = duration;
	return p;
}

static void test_pitch_to_hz() {
	CHECK_EQ(SoundAdapter::pitch_to_hz(1), 1313);
	CHECK_EQ(SoundAdapter::pitch_to_hz(11), 608);
	CHECK_EQ(SoundAdapter::pitch_to_hz(0), 43);       // 0 wraps to 256
}

static void test_timed_tone() {
	now = 1000;
	trace_sound trace(clk);
	SoundAdapter snd(trace, clk);
	snd.play(beep(11, 1000));                          // 72,000 us = 72 ms
	CHECK_EQ(trace.events().size(), 1);
	CHECK(trace.events()[0].hz == 608 && trace.events()[0].ms == 72);
	now = 1000 + 71999;  CHECK(snd.active());
	now = 1000 + 72000;  CHECK(!snd.active());
}

static void test_short_duration_rounds_up() {
	now = 0;
	trace_sound trace(clk);
	SoundAdapter snd(trace, clk);
	snd.play(beep(11, 1));                             // 72 us
	CHECK_EQ(trace.events()[0].ms, 1);                 // never 0, which means "forever"
}

static void test_zero_duration_until_stopped() {
	now = 0;
	trace_sound trace(clk);
	SoundAdapter snd(trace, clk);
	snd.play(beep(11, 0));
	CHECK_EQ(trace.events()[0].ms, 0);
	now += 100000000;  CHECK(snd.active());
	snd.stop();
	CHECK(!snd.active() && trace.events().back().hz == 0);
}

static void test_stop_when_idle_is_silent() {
	trace_sound trace(clk);
	SoundAdapter snd(trace, clk);
	snd.stop();
	CHECK(trace.events().empty());
}

static void test_new_beep_replaces_old() {
	now = 0;
	trace_sound trace(clk);
	SoundAdapter snd(trace, clk);
	snd.play(beep(11, 1000));
	now = 10000;
	snd.play(beep(50, 2000));
	CHECK(trace.events().size() == 2 && trace.events()[1].hz != 608);
	now = 10000 + 143999;  CHECK(snd.active());       // 2000 * 72 us from the second play
}

static void test_clock_wraparound() {
	now = UINT32_MAX - 100;
	trace_sound trace(clk);
	SoundAdapter snd(trace, clk);
	snd.play(beep(11, 1000));
	now = (UINT32_MAX - 100) + 71999;  CHECK(snd.active());
	now = (UINT32_MAX - 100) + 72000;  CHECK(!snd.active());
}

static SoundAdapter::Params sweep(uint8_t p1, uint8_t p2, uint16_t interval, int8_t step) {
	SoundAdapter::Params p{};
	p.pitch1 = p1;
	p.pitch2 = p2;
	p.interval = interval;
	p.step = step;
	return p;
}

static uint32_t us(uint16_t units) { return uint32_t(units) * 72; }   // 72 us per unit

static void test_no_sweep_when_pitches_equal() {
	auto p = sweep(20, 20, 1000, 4);
	CHECK_EQ(SoundAdapter::pitch_at(p, 0), 20);
	CHECK_EQ(SoundAdapter::pitch_at(p, us(5000)), 20);
}

static void test_no_sweep_when_interval_zero() {
	auto p = sweep(20, 100, 0, 4);
	CHECK_EQ(SoundAdapter::pitch_at(p, us(5000)), 20);
}

static void test_no_sweep_when_step_zero() {
	auto p = sweep(20, 100, 1000, 0);
	CHECK_EQ(SoundAdapter::pitch_at(p, us(5000)), 20);
}

static void test_no_sweep_when_pitch2_is_a_higher_tone() {
	auto p = sweep(100, 20, 1000, 4);          // pitch2 numerically lower = higher tone
	CHECK_EQ(SoundAdapter::pitch_at(p, us(5000)), 100);
}

static void test_first_leg_of_sweep() {
	// HYPOTHESIS: from pitch1, the IPC pitch byte moves by `step` once per `interval`,
	// the first step landing at t = interval, heading for pitch2. First leg only.
	auto p = sweep(20, 100, 1000, 4);
	uint32_t iv = us(1000);
	CHECK_EQ(SoundAdapter::pitch_at(p, 0), 20);
	CHECK_EQ(SoundAdapter::pitch_at(p, iv - 1), 20);       // not yet
	CHECK_EQ(SoundAdapter::pitch_at(p, iv), 24);
	CHECK_EQ(SoundAdapter::pitch_at(p, 2 * iv), 28);
	CHECK_EQ(SoundAdapter::pitch_at(p, 10 * iv), 60);      // still short of pitch2
}

int main() {
	RUN(test_pitch_to_hz);
	RUN(test_timed_tone);
	RUN(test_short_duration_rounds_up);
	RUN(test_zero_duration_until_stopped);
	RUN(test_stop_when_idle_is_silent);
	RUN(test_new_beep_replaces_old);
	RUN(test_clock_wraparound);
	RUN(test_no_sweep_when_pitches_equal);
	RUN(test_no_sweep_when_interval_zero);
	RUN(test_no_sweep_when_step_zero);
	RUN(test_no_sweep_when_pitch2_is_a_higher_tone);
	RUN(test_first_leg_of_sweep);
	return failures != 0;
}
