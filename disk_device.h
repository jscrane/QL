#pragma once

#define DISK_DRIVER_ADDR	0x0c000
#define DISK_DEVICE_ADDR	(DISK_DRIVER_ADDR + 0x1000)
#define DISK_OPEN_OFFSET	0x00
#define DISK_CLOSE_OFFSET	0x04
#define DISK_IO_OFFSET		0x08

#define DISK_OPEN	(DISK_DEVICE_ADDR + DISK_OPEN_OFFSET)
#define DISK_CLOSE	(DISK_DEVICE_ADDR + DISK_CLOSE_OFFSET)
#define DISK_IO		(DISK_DEVICE_ADDR + DISK_IO_OFFSET)

#ifndef __ASSEMBLER__

class DiskDevice: public Memory::Device {
public:
	DiskDevice(m68k &cpu): Memory::Device(256), _cpu(cpu) {}

	void operator=(uint8_t) override {}
	operator uint8_t() override;

	void checkpoint(Checkpoint &) override;
	void restore(Checkpoint &) override;

private:
	uint8_t do_open(uint32_t type, Memory::address chan_def, Memory::address phys_def, Memory::address link_blk, Memory::address sys_vars);
	uint8_t do_close(Memory::address chan_def, Memory::address phys_def, Memory::address link_blk, Memory::address sys_vars);
	uint8_t do_io();

	m68k &_cpu;
};

#endif
