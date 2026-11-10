#include <cstdint>
#include <cstdio>

#include "check.h"
#include "trace_sound.h"
#include "snd_adapter.h"

static uint32_t now;
static uint32_t clk() { return now; }

static SoundAdapter::Params beep(uint8_t pitch1, uint8_t pitch2, uint16_t interval, uint16_t duration,
		int8_t step, uint8_t wrap) {
	SoundAdapter::Params p{};
	p.pitch1 = pitch1;
	p.pitch2 = pitch2;
	p.interval = interval;
	p.duration = duration;
	p.step = step;
	p.wrap = wrap;
	return p;
}

// BEEP duration,pitch as the IPC receives it: pitch + 1 on the wire
static SoundAdapter::Params simple(uint16_t duration, uint8_t pitch) {
	return beep(pitch + 1, pitch + 1, 0, duration, 0, 0);
}

struct Rig {
	trace_sound trace;
	SoundAdapter snd;
	Rig(): trace(clk), snd(trace, clk) { now = 1000; }
	void run_ms(unsigned ms) {
		for (unsigned i = 0; i < ms; i++) {
			now += 1000;
			snd.poll();
		}
	}
};

static void test_pitch_to_hz() {
	CHECK_EQ(SoundAdapter::pitch_to_hz(1), 1310);
	CHECK_EQ(SoundAdapter::pitch_to_hz(11), 611);
	CHECK_EQ(SoundAdapter::pitch_to_hz(0), 43);            // 0 wraps to 256
}

static void test_untimed_tone_until_stopped() {
	Rig r;
	r.snd.play(simple(0, 10));                             // BEEP 0,10
	CHECK_EQ(r.trace.events().size(), 1);
	CHECK_EQ(r.trace.events()[0].hz, 611);
	r.run_ms(5000);
	CHECK(r.snd.active());
	CHECK_EQ(r.trace.events().size(), 1);                  // polling must not retune a steady tone
	r.snd.stop();
	CHECK(!r.snd.active());
	CHECK_EQ(r.trace.events().back().hz, 0);
}

static void test_timed_tone_ends() {
	Rig r;
	r.snd.play(simple(1000, 10));                          // BEEP 1000,10: about 53 ms
	CHECK_EQ(r.trace.events()[0].hz, 560);                 // a timed tone sounds lower
	r.run_ms(50);
	CHECK(r.snd.active());
	r.run_ms(6);
	CHECK(!r.snd.active());
	CHECK_EQ(r.trace.events().back().hz, 0);
}

static void test_active_is_not_stale_without_polling() {
	Rig r;
	r.snd.play(simple(1000, 10));
	now += 56000;                                          // nobody polled
	CHECK(!r.snd.active());                                // active() advances the model itself
	CHECK_EQ(r.trace.events().back().hz, 0);
}

static void test_never_ending_duration() {
	Rig r;
	r.snd.play(simple(0xFFFF, 50));                        // BEEP -1,50: this one steps over its target
	r.run_ms(6500);
	CHECK(r.snd.active());
	r.snd.stop();
	CHECK(!r.snd.active());
}

static void test_replace_and_stop() {
	Rig r;
	r.snd.play(simple(0, 10));
	r.snd.play(simple(0, 40));                             // replaces the first
	CHECK_EQ(r.trace.events().size(), 2);
	CHECK(r.trace.events()[1].hz < r.trace.events()[0].hz);
	r.snd.stop();
	r.snd.stop();                                          // a second stop is silent
	CHECK_EQ(r.trace.events().size(), 3);
}

static void test_sweep_followed_at_1ms_polling() {
	// pitch 20..30 step 4 every 1000 units (about 45 ms): 20 24 28 32 28 24 20 as pitch values
	Rig r;
	r.snd.play(beep(20, 30, 1000, 0, 4, 0));
	r.run_ms(300);
	const auto &e = r.trace.events();
	CHECK_EQ(e.size(), 7);
	if (e.size() == 7) {
		CHECK(e[0].hz > e[1].hz && e[1].hz > e[2].hz && e[2].hz > e[3].hz);   // pitch rising = tone falling
		CHECK(e[3].hz < e[4].hz && e[4].hz < e[5].hz && e[5].hz < e[6].hz);   // after the turn
	}
}

static void test_fast_sweep_retunes_at_most_once_per_poll() {
	Rig r;
	r.snd.play(beep(20, 60, 0, 0, 4, 0));                  // interval 0: a step every half-cycle
	r.run_ms(60);
	CHECK(r.trace.events().size() > 5);
	CHECK(r.trace.events().size() <= 61);
}

int main() {
	RUN(test_pitch_to_hz);
	RUN(test_untimed_tone_until_stopped);
	RUN(test_timed_tone_ends);
	RUN(test_active_is_not_stale_without_polling);
	RUN(test_never_ending_duration);
	RUN(test_replace_and_stop);
	RUN(test_sweep_followed_at_1ms_polling);
	RUN(test_fast_sweep_retunes_at_most_once_per_poll);
	std::printf("%s\n", failures ? "FAILED" : "all passed");
	return failures != 0;
}
