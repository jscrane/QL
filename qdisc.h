#pragma once

class QDisc: public Memory::Device {
public:
	QDisc(): Memory::Device(256) {}

	void operator=(uint8_t) override;
	operator uint8_t() override;

	void checkpoint(Checkpoint &) override;
	void restore(Checkpoint &) override;

private:
	uint8_t _wd_status = 0;
	uint8_t _wd_index_reads = 0;
};
