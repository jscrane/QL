// Tests for the half-cycle model of the QL's IPC sound process.
// The golden vectors in ipc_sound_vectors.h were produced by running the real IPC firmware
// (gen_sound_vectors.py + ipc8049_sim.py), so passing them means the model reproduces it.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "check.h"
#include "beep_model.h"
#include "ipc_sound_vectors.h"

static BeepParams make(uint8_t p1, uint8_t p2, uint16_t interval, uint16_t duration, int8_t step, uint8_t wrap) {
	BeepParams p{};
	p.pitch1 = p1;
	p.pitch2 = p2;
	p.interval = interval;
	p.duration = duration;
	p.step = step;
	p.wrap = wrap;
	return p;
}

// pitch values of the first n runs (consecutive half-cycles at one pitch)
static std::vector<int> pitches(const BeepParams &p, unsigned n) {
	std::vector<int> out;
	BeepModel m;
	m.start(p);
	for (unsigned i = 0; i < 20000 && m.running() && out.size() < n; i++) {   // capped: a steady tone has one run
		if (out.empty() || out.back() != m.pitch())
			out.push_back(m.pitch());
		m.next();
	}
	return out;
}

static bool same(const std::vector<int> &got, std::vector<int> want) {
	if (got != want) {
		std::printf("   got:");
		for (int g : got) std::printf(" %d", g);
		std::printf("\n  want:");
		for (int w : want) std::printf(" %d", w);
		std::printf("\n");
	}
	return got == want;
}

// ---- readable statements of the semantics (the vectors below check them against the firmware)

static void test_steady_hz() {
	CHECK_EQ(BeepModel::steady_hz(1), 1310);            // BEEP 0,0
	CHECK_EQ(BeepModel::steady_hz(11), 611);            // BEEP 0,10
	CHECK_EQ(BeepModel::steady_hz(0), 43);              // BEEP 0,255: the byte wraps to 256
	CHECK_EQ(BeepModel::steady_hz(11, true), 560);      // a timed tone sounds lower
}

static void test_bounce() {
	// the pitch that leaves the range is played once, then the direction reverses
	CHECK(same(pitches(make(20, 30, 1000, 0, 4, 0), 8), {20, 24, 28, 32, 28, 24, 20, 24}));
}

static void test_wrap_1() {
	// one restart from the start pitch (replacing the overshoot), then it bounces
	CHECK(same(pitches(make(20, 30, 1000, 0, 4, 1), 9), {20, 24, 28, 20, 24, 28, 32, 28, 24}));
}

static void test_wrap_15() {
	CHECK(same(pitches(make(20, 30, 1000, 0, 4, 15), 8), {20, 24, 28, 20, 24, 28, 20, 24}));
}

static void test_negative_step_starts_at_pitch2() {
	CHECK(same(pitches(make(20, 30, 1000, 0, -4, 0), 7), {30, 26, 22, 18, 22, 26, 30}));
}

static void test_equal_pitches_trill() {
	CHECK(same(pitches(make(20, 20, 1000, 0, 4, 0), 6), {20, 24, 20, 24, 20, 24}));
}

static void test_step_zero_is_steady() {
	CHECK(same(pitches(make(20, 30, 1000, 0, 0, 0), 2), {20}));
}

// ---- golden vectors

static bool within(uint32_t got, uint32_t want) {
	uint32_t tol = want / 50 > 300 ? want / 50 : 300;       // 2%, at least 0.3 ms
	return got + tol >= want && got <= want + tol;
}

static void check_vector(const SoundVector &v) {

	BeepModel m;
	m.start(make(v.pitch1, v.pitch2, v.interval, v.duration, v.step, v.wrap));
	const uint64_t window = BeepModel::us_to_cycles(v.window_us);

	struct Run { uint8_t pitch; unsigned n; uint64_t start; };
	std::vector<Run> runs;
	while (m.running() && m.started() < window) {
		if (runs.empty() || runs.back().pitch != m.pitch())
			runs.push_back({ m.pitch(), 1, m.started() });
		else
			runs.back().n++;
		m.next();
	}
	const bool ended = !m.running();
	const uint32_t ended_us = BeepModel::cycles_to_us(m.started());

	double worst = 0;
	for (unsigned i = 0; i < v.nruns; i++) {
		const SoundRun &want = v.runs[i];
		if (i >= runs.size()) {
			++failures;
			std::printf("%s: model has only %zu runs, vector has %u\n", v.name, runs.size(), v.nruns);
			return;
		}
		if (runs[i].pitch != want.pitch) {
			++failures;
			std::printf("%s: run %u pitch %u, want %u\n", v.name, i, runs[i].pitch, want.pitch);
			return;
		}
		if (!want.complete || i + 1 >= runs.size())
			continue;
		const uint32_t us = BeepModel::cycles_to_us(runs[i + 1].start - runs[i].start);
		if (runs[i].n != want.half_cycles) {
			++failures;
			std::printf("%s: run %u has %u half-cycles, want %u\n", v.name, i, runs[i].n, want.half_cycles);
		}
		if (!within(us, want.us)) {
			++failures;
			std::printf("%s: run %u lasts %u us, want %u\n", v.name, i, us, want.us);
		}
		double dev = (double(us) - want.us) / want.us;
		if (dev < 0) dev = -dev;
		if (dev > worst) worst = dev;
	}
	if (v.ends_us) {
		if (!ended || !within(ended_us, v.ends_us)) {
			++failures;
			std::printf("%s: ended=%d at %u us, want %u\n", v.name, ended, ended_us, v.ends_us);
		}
	} else if (ended) {
		++failures;
		std::printf("%s: model ended at %u us but the firmware was still sounding\n", v.name, ended_us);
	}
	std::printf("  %-28s worst run deviation %.2f%%\n", v.name, worst * 100);
}

static void test_vectors() {
	for (unsigned i = 0; i < n_sound_vectors; i++)
		check_vector(sound_vectors[i]);
}

int main() {
	RUN(test_steady_hz);
	RUN(test_bounce);
	RUN(test_wrap_1);
	RUN(test_wrap_15);
	RUN(test_negative_step_starts_at_pitch2);
	RUN(test_equal_pitches_trill);
	RUN(test_step_zero_is_steady);
	RUN(test_vectors);
	std::printf("%s\n", failures ? "FAILED" : "all passed");
	return failures != 0;
}
