#pragma once

#define BG_COLOUR       BLACK
#define FG_COLOUR       WHITE

#if defined(USE_ESPI)
#define ORIENT          reverse_landscape
#else
#define ORIENT          portrait
#endif

#define PROGRAMS	"/QL"

#define USER_RAM	0x38000
