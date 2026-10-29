#pragma once

class flash_file;

class wd1770 {
public:
	wd1770(flash_file &image): _image(image) {}
	void reset();

	uint8_t read_status();
	uint8_t read_track() const { return _wd_track; }
	uint8_t read_sector() const { return _wd_sector; }
	uint8_t read_data();

	void write_command(uint8_t);
	void write_track(uint8_t b) { _wd_track = b; }
	void write_sector(uint8_t b) { _wd_sector = b; }
	void write_data(uint8_t b) { _wd_data = b; }

	void set_side(uint8_t b) { _side = b; }
	void set_drive(uint8_t b) { /* FIXME */ }

private:
	uint32_t offset() const;
	bool can_seek() const;

	flash_file &_image;
	uint8_t _wd_status = 0;
	uint8_t _wd_command_type = 0;
	uint8_t _wd_index_reads = 0;
	uint8_t _wd_track = 0;
	int _wd_direction = -1;
	uint8_t _wd_sector = 1;
	uint8_t _wd_data = 0;
	uint16_t _wd_sector_pos = 0;
	bool _wd_read_active = false;
	uint8_t _wd_last_cmd = 0;
	uint8_t _side = 0;
};
