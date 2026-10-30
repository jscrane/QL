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

Memory memory;
m68k cpu(memory);
Arduino machine(cpu);

hw_serial_kbd kbd(Serial);
Keyboard keyboard(kbd);
IO io(keyboard);
Screen screen;

flash_filer images(PROGRAMS, 2);
flash_file drive1(0), drive2(1);
QDisc floppy(drive1, drive2);

bool debug_cpu = false;

static void file_status() {
	static const char *device_names[] = { "D1:", "D2:" };
	const char *image = images.filename();
	char buf[80];
	strcpy(buf, device_names[images.device()]);
	strcat(buf, image? image: "No image selected");
	screen.status(buf);
}

static void function_keys(uint8_t key) {

	switch (key) {
	case 6:
		machine.reset();
		break;
	case 7:
		images.advance();
		break;
	case 8:
		images.rewind();
		break;
	case 9:
		images.next_device();
		break;
	}
	file_status();
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
	kbd.handle_fnkeys(F6 | F7 | F8 | F9);
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

	machine.run(CLK_8MHZ);
}
