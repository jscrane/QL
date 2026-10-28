#pragma once

class QDisc: public Memory::Device {
public:
	QDisc(flash_file &image): Memory::Device(256), _image(image) {}
	void reset();

	void operator=(uint8_t) override;
	operator uint8_t() override;

	void checkpoint(Checkpoint &) override;
	void restore(Checkpoint &) override;

private:
	uint8_t read_status();
	uint8_t read_data();
	void write_command(uint8_t);
	int disk_side() const;
	int disk_drive() const;
	uint32_t offset() const;

	flash_file &_image;
	uint8_t _wd_status = 0;
	uint8_t _wd_command_type = 0;
	uint8_t _latch = 0;
	uint8_t _wd_index_reads = 0;
	uint8_t _wd_track = 0;
	int _wd_direction = -1;
	uint8_t _wd_sector = 1;
	uint8_t _wd_data = 0;
	uint16_t _wd_sector_pos = 0;
	bool _wd_read_active = false;
	uint8_t _wd_last_cmd = 0;
};
