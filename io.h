#pragma once

class IO: public Memory::Device, public Pollable {
public:
	IO(m68k &cpu, serial_kbd &kbd): Memory::Device(0x10000), _cpu(cpu), _kbd(kbd) {}

	void operator=(uint8_t) override;
	operator uint8_t() override;

	void checkpoint(Checkpoint &) override;
	void restore(Checkpoint &) override;

	void poll() override;

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

	uint8_t interrupt_status();
	void link_command(uint8_t);
	void receive_command_bit(uint8_t);
	void send_response_bit(uint8_t);

	m68k &_cpu;
	serial_kbd &_kbd;

	enum IPCState {
		Command, Response
	};

	IPCState _ipc_state = Command;

	bool _has_key;
	uint8_t _key;

	uint8_t _cmd = 0, _cmd_bits = 0;
	uint8_t _rsp_bit = 0;
	uint64_t _rsp;
	uint8_t _comdata = 0;

	bool _debugging = false;
};
