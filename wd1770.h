#pragma once

class flash_file;

class wd1770 {
public:
	wd1770(flash_file &d1, flash_file &d2): _d1(d1), _d2(d2), _image(&d1) {}
	void reset();

	uint8_t read_status();
	uint8_t read_track() const { return _track; }
	uint8_t read_sector() const { return _sector; }
	uint8_t read_data();

	void write_command(uint8_t);
	void write_track(uint8_t b) { _track = b; }
	void write_sector(uint8_t b) { _sector = b; }
	void write_data(uint8_t);

	void set_side(uint8_t b) { _side = b; }
	void set_drive(uint8_t b);

private:
	void end_command(uint8_t);
	void start_sector(bool, bool);
	void next_sector();
	void step(int, uint8_t);
	bool seek();

	flash_file &_d1, &_d2;
	flash_file *_image;
	uint8_t _status = 0;
	uint8_t _command_type = 0;
	uint16_t _index_reads = 0;
	uint8_t _track = 0;
	int _direction = -1;
	uint8_t _sector = 1;
	uint8_t _data = 0;
	uint16_t _sector_pos = 0;
	bool _read_active = false;
	bool _write_active = false;
	bool _multi = false;
	uint8_t _side = 0;
};
