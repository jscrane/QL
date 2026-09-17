#pragma once

class IO: public Memory::Device {
public:
	IO(): Memory::Device(0x10000) {}

	void operator=(uint8_t) override;
	operator uint8_t() override;

	void checkpoint(Checkpoint &) override;
	void restore(Checkpoint &) override;

	void set_display_ctrl(std::function<void(uint8_t)> fn) { _display_ctrl = fn; }
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

	std::function<void(uint8_t)> _display_ctrl;
};
