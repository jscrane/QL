#include <r65emu.h>
#include <m68k.h>
#include <dram.h>

#include "keyboard.h"
#include "microdrive.h"
#include "io.h"
#include "screen.h"
#include "js.h"
#include "tkii220.h"
#include "cst_qdisc_116.h"
#include "qdisc.h"

dram<0x18000> user_ram;
prom qdos(js, sizeof(js));
prom tkii(tkii220, sizeof(tkii220));
prom qdisc(cst_qdisc_116, sizeof(cst_qdisc_116));

Memory memory;
m68k cpu(memory);
Arduino machine(cpu);
Keyboard keyboard(Serial);
IO io(keyboard);
Screen screen;
QDisc floppy;

bool debug_cpu = false;

void setup() {

	machine.begin();
	user_ram.begin();
	screen.begin();

	io.set_display_ctrl([](uint8_t b) { screen.set_mode(b); });
	io.set_interrupt_cpu([](bool b) { cpu.set_interrupt_level(b? 2: 0); });
	io.begin();

	memory.put(qdos, 0x00000);
	memory.put(tkii, 0x0c000);
	memory.put(io, 0x10000);
	memory.put(screen, 0x20000);
	memory.put(user_ram, 0x28000);
	memory.put(qdisc, 0xc0000);
	memory.put(floppy, 0xc3f00);

	machine.set_cpu_debugging([]() { return debug_cpu; });
	machine.register_pollable(io);
	machine.reset();
}

void loop() {

	machine.run(1);
}
