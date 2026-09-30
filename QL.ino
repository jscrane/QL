#include <r65emu.h>
#include <m68k.h>
#include <dram.h>

#include "jsrom.h"
#include "keyboard.h"
#include "microdrive.h"
#include "io.h"
#include "screen.h"
#include "disk.h"
#include "disk_device.h"

dram<0x18000> user_ram;
prom qdos(jsrom, sizeof(jsrom));
prom disk_driver(disk, sizeof(disk));

Memory memory;
m68k cpu(memory);
Arduino machine(cpu);
Keyboard keyboard(Serial);
IO io(keyboard);
Screen screen;
DiskDevice disk_device(cpu);

bool debug_cpu = false;

void setup() {

	machine.begin();
	user_ram.begin();
	screen.begin();

	io.set_display_ctrl([](uint8_t b) { screen.set_mode(b); });
	io.set_interrupt_cpu([](bool b) { cpu.set_interrupt_level(b? 2: 0); });
	io.begin();

	memory.put(qdos, 0x00000);
	memory.put(disk_driver, DISK_DRIVER_ADDR);
	memory.put(disk_device, DISK_DEVICE_ADDR);
	memory.put(io, 0x10000);
	memory.put(screen, 0x20000);
	memory.put(user_ram, 0x28000);

	//machine.set_cpu_debugging([]() { return debug_cpu; });
	machine.set_cpu_debugging([]() { return cpu.pc() >= DISK_DRIVER_ADDR && cpu.pc() < DISK_DEVICE_ADDR; });
	machine.register_pollable(io);
	machine.reset();
}

void loop() {

	machine.run(1);
}
