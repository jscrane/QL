#include <cstdint>

#include <machine.h>
#include <debugging.h>
#include <memory.h>
#include <CPU.h>
#include <m68k.h>

#include "disk_device.h"

DiskDevice::operator uint8_t() {

	switch (_acc) {
	case DISK_OPEN_OFFSET:
		return do_open(_cpu.d(3), _cpu.a(0), _cpu.a(1), _cpu.a(3), _cpu.a(6));

	case DISK_CLOSE_OFFSET:
		return do_close(_cpu.a(0), _cpu.a(1), _cpu.a(3), _cpu.a(6));

	case DISK_IO_OFFSET:
		return do_io();
	}

	return 0xff;
}

uint8_t DiskDevice::do_open(uint32_t type, Memory::address chan_def, Memory::address phys_def, Memory::address link_blk, Memory::address sys_vars) {

	DBG_EMU("do_open: %08x %06x %06x %06x", type, chan_def, phys_def, link_blk, sys_vars);
	// FIXME
	return -1;
}

uint8_t DiskDevice::do_close(Memory::address chan_def, Memory::address phys_def, Memory::address link_blk, Memory::address sys_vars) {

	DBG_EMU("do_close: %06x %06x %06x", chan_def, phys_def, link_blk, sys_vars);
	// FIXME
	return -1;
}

uint8_t DiskDevice::do_io() {

	DBG_EMU("do_io");
	// FIXME
	return -1;
}

void DiskDevice::checkpoint(Checkpoint &) {
	// FIXME
}

void DiskDevice::restore(Checkpoint &) {
	// FIXME
}
