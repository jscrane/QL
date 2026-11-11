// fake_clock.h
#pragma once
#include <cstdint>

inline uint32_t now = 0;
inline uint32_t clk() { return now; }
