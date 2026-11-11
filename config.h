#pragma once

#define BG_COLOUR       BLACK
#define FG_COLOUR       WHITE

#if defined(USE_ESPI)
#define ORIENT          reverse_landscape
#else
#define ORIENT          portrait
#endif

#define PROGRAMS	"/QL"

#define TOTAL_RAM_KB	640

// screen has its own 32kB
#define USER_RAM	(TOTAL_RAM_KB - 32) * 1024
