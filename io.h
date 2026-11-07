#pragma once

class IPC;
class Microdrive;

class IO: public Memory::Device, public Pollable {
public:
	IO(IPC &ipc, Microdrive &mdv): Memory::Device(0x10000), _ipc(ipc), _mdv(mdv) {}
	void begin();
	void reset();

	void operator=(uint8_t) override;
	operator uint8_t() override;

	void checkpoint(Checkpoint &) override;
	void restore(Checkpoint &) override;

	void poll() override;

	void set_display_ctrl(std::function<void(uint8_t)> fn) { _display_ctrl = fn; }
	void set_interrupt_cpu(std::function<void(bool)> fn) { _interrupt_cpu = fn; }
private:
	// write
	static constexpr uint16_t RTC_RESET = 0x8000;
	static constexpr uint16_t RTC_STEP = 0x8001;
	static constexpr uint16_t TRANSMIT_CTRL = 0x8002;
	static constexpr uint16_t IPC_LINK_CTRL = 0x8003;
	static constexpr uint16_t MDV_CTRL = 0x8020;
	static constexpr uint16_t INT_CTRL = 0x8021;
	static constexpr uint16_t MDV_RS_DATA = 0x8022;
	static constexpr uint16_t DISPLAY_CTRL = 0x8063;

	// read
	static constexpr uint16_t RTC_BYTE0 = 0x8000;
	static constexpr uint16_t RTC_BYTE1 = 0x8001;
	static constexpr uint16_t RTC_BYTE2 = 0x8002;
	static constexpr uint16_t RTC_BYTE3 = 0x8003;
	static constexpr uint16_t MDV_RS_STAT = 0x8020;
	static constexpr uint16_t INT_IPC_STAT = 0x8021;
	static constexpr uint16_t MDV_DATA_TRK1 = 0x8022;
	static constexpr uint16_t MDV_DATA_TRK2 = 0x8023;

	// interrupt bits for 0x8021
	static constexpr uint8_t INT_EXTERNAL = (1 << 4);
	static constexpr uint8_t INT_FRAME = (1 << 3);
	static constexpr uint8_t INT_TRANSMIT = (1 << 2);
	static constexpr uint8_t INT_INTERFACE = (1 << 1);
	static constexpr uint8_t INT_GAP = (1 << 0);

	static constexpr uint8_t MASK_TRANSMIT  = (1 << 7);
	static constexpr uint8_t MASK_INTERFACE = (1 << 6);
	static constexpr uint8_t MASK_GAP = (1 << 5);

	std::function<void(uint8_t)> _display_ctrl;
	std::function<void(bool)> _interrupt_cpu;

	void interrupt_acknowledge(uint8_t);


	uint8_t _irq_status = 0, _irq_mask = 0;
	uint32_t _rtc = 0;

	IPC &_ipc;
	Microdrive &_mdv;
};
