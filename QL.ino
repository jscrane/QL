#include <r65emu.h>
#include <m68k.h>
#include <dram.h>

#include "jsrom.h"
#include "keyboard.h"
#include "io.h"
#include "screen.h"

dram<0x18000> user_ram;
prom qdos(jsrom, sizeof(jsrom));

Memory memory;
m68k cpu(memory);
Arduino machine(cpu);
Keyboard keyboard(Serial);
IO io(cpu, keyboard);
Screen screen;

bool debug_cpu = false;

void setup() {

	machine.begin();
	user_ram.begin();
	screen.begin();

	io.set_display_ctrl([](uint8_t b) { screen.set_mode(b); });

	memory.put(qdos, 0x00000);
	memory.put(io, 0x10000);
	memory.put(screen, 0x20000);
	memory.put(user_ram, 0x28000);

	machine.set_cpu_debugging([]() { return debug_cpu; });
	machine.register_pollable(io);
	machine.reset();
}

void loop() {
	machine.run(1);
}
