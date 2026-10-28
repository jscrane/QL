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
#include "config.h"

dram<0x18000> user_ram;
prom qdos(js, sizeof(js));
prom tkii(tkii220, sizeof(tkii220));
prom qdisc(cst_qdisc_116, sizeof(cst_qdisc_116));
hw_serial_kbd kbd(Serial);

Memory memory;
m68k cpu(memory);
Arduino machine(cpu);
Keyboard keyboard(kbd);
IO io(keyboard);
Screen screen;
flash_filer images(PROGRAMS);
QDisc floppy(images);

bool debug_cpu = false;

static void function_keys(uint8_t key) {

	static const char *disk_image;

	switch (key) {
	case 6:
		machine.reset();
		break;
	case 7:
		disk_image = images.advance();
		screen.status(disk_image? disk_image: "No Disk");
		break;
	case 8:
		disk_image = images.rewind();
		screen.status(disk_image? disk_image: "No Disk");
		break;
	}
}

static void reset(bool sd) {

	if (!sd)
		screen.status("No SD Card");
	else if (!images.start())
		screen.status("Failed to open " PROGRAMS);

	floppy.reset();
	debug_cpu = false;
}

void setup() {

	machine.begin();
	user_ram.begin();
	screen.begin();
	kbd.handle_fnkeys(F6 | F7 | F8);
	kbd.register_fnkey_handler(function_keys);

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
//	machine.set_cpu_debugging([]() { return cpu.pc() >= 0xc10ac && cpu.pc() <= 0xc10ee; });
//	machine.set_cpu_debugging([]() { return cpu.pc() >= 0xc12a6 && cpu.pc() <= 0xc12b8; });
	machine.register_pollable(io);
	machine.register_reset_handler(reset);
	machine.reset();
}

void loop() {

	machine.run(CLK_4MHZ);
}
