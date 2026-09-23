#pragma once

class IO: public Memory::Device, public Pollable {
public:
	IO(m68k &cpu, Keyboard &kbd): Memory::Device(0x10000), _cpu(cpu), _kbd(kbd) {}
	void begin();

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

	// interrupt bits for 0x8021
	static constexpr uint8_t INT_BAUD = (1 << 7);
	static constexpr uint8_t INT_MDI  = (1 << 6);
	static constexpr uint8_t INT_RTC  = (1 << 5);
	static constexpr uint8_t INT_EXT  = (1 << 4);
	static constexpr uint8_t INT_GAP  = (1 << 3);
	static constexpr uint8_t INT_TX   = (1 << 2);
	static constexpr uint8_t INT_IPC  = (1 << 1);
	static constexpr uint8_t INT_MDG  = (1 << 0);

	static constexpr uint8_t IM_RTC = (1 << 7);
	static constexpr uint8_t IM_EXT = (1 << 6);
	static constexpr uint8_t IM_GAP = (1 << 5);
	static constexpr uint8_t IC_EXT  = INT_EXT;
	static constexpr uint8_t IC_GAP  = INT_GAP;
	static constexpr uint8_t IC_TX   = INT_TX;
	static constexpr uint8_t IC_IPC  = INT_IPC;
	static constexpr uint8_t IC_MDG  = INT_MDG;

	std::function<void(uint8_t)> _display_ctrl;

	void interrupt_acknowledge(uint8_t);
	void link_command(uint8_t);
	void receive_command_bit(uint8_t);
	void send_response_bit(uint8_t);

	m68k &_cpu;
	Keyboard &_kbd;

	enum IPCState {
		Start, Command, Response
	};

	IPCState _ipc_state = Start;

	uint8_t _cmd, _cmd_bits;
	uint16_t _rsp, _rsp_bit;
	uint8_t _comdata;
	uint8_t _irq_status = 0, _irq_mask = 0;

	bool _debugging = false;
};
