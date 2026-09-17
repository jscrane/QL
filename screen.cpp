#include <cstdint>
#include <malloc.h>

#include <machine.h>
#include <memory.h>
#include <dram.h>
#include <display.h>
#include <debugging.h>

#include "screen.h"
#include "config.h"

//
// the QL screen consists of 256 lines of 128 bytes
// there are 2 modes: mode 4 (512 pixels wide) and mode 8 (256 pixels)
// there are 2 screens: starting at $20000 and $28000
//
// mode 4: 4 colours, 2 bits / pixel.
//   each 16-bit word represents 8 pixels:
//   - addr + 0: G7 G6 G5 G4 G3 G2 G1 G0	(high byte)
//   - addr + 1: R7 R6 R5 R4 R3 R2 R1 R0	(low byte)
//   - where GR: 0b00=black, 0b10=green, 0b01=red, 0b11=white
//   - pixel 0 is G7R7 ... pixel 7 is G0R0
//
// mode 8: 8 colours (and flashing), 4 bits / pixel
//   each 16-bit word represents 4 pixels:
//   - addr + 0: G3 F3 G2 F2 G1 F1 G0 F0	(high byte)
//   - addr + 1: R3 B3 R2 B2 R1 B1 R0 B0	(low byte)
//   - where RGB: 0b000=black 0b001=blue 0b010=green 0b011=cyan
//                0b100=red 0b101=magenta 0b110=yellow 0b111=white
//   - and F is flash on / off
//   - pixel 0 is R3G3B3F3 ... pixel 3 is R0G0B0F0
//
void Screen::operator=(uint8_t b) {

	_ram.set(_acc, b);
	
	if (is_visible(_acc))
		draw(_acc);
}

void Screen::draw(Memory::address addr) {

	uint8_t line = addr >> 7;
	uint8_t word = (addr >> 1) & 0x3f;
	if (_mode4)
		draw_mode4(addr, line, word);
	else
		draw_mode8(addr, line, word);
}

void Screen::redraw() {

	Memory::address off = screen_base();
	for (uint16_t i = 0; i < 0x8000; i += 2)
		draw(off + i);
}

void Screen::draw_mode4(Memory::address addr, uint8_t line, uint8_t word) {

	uint8_t green = _ram.get(addr & ~1), red = _ram.get(addr | 1);
	uint16_t x = word * 8;

	for (int i = 0; i < 8; i++) {
		uint8_t bit = 0x80 >> i;
		bool g = green & bit, r = red & bit;
		uint16_t col;

		if (!g && !r) col = BLACK;
		else if (g & !r) col = GREEN;
		else if (!g && r) col = RED;
		else if (g && r) col = WHITE;

		_dsp.drawPixel(x + i, line, col);
	}
}

void Screen::draw_mode8(Memory::address addr, uint8_t line, uint8_t word) {

	uint8_t green = _ram.get(addr & ~1);
	uint8_t redblue = _ram.get(addr | 1);
	uint16_t x = word * 4;

	for (int i = 0; i < 4; i++) {
		uint8_t shift = 6 - i * 2;

		bool g = green    & (1 << (shift + 1));
		bool r = redblue  & (1 << (shift + 1));
		bool b = redblue  & (1 << shift);

		uint8_t rgb = (r << 2) | (g << 1) | b;

		uint16_t col;

		switch (rgb) {
		case 0b000: col = BLACK;   break;
		case 0b001: col = BLUE;    break;
		case 0b010: col = GREEN;   break;
		case 0b011: col = CYAN;    break;
		case 0b100: col = RED;     break;
		case 0b101: col = MAGENTA; break;
		case 0b110: col = YELLOW;  break;
		case 0b111: col = WHITE;   break;
		}

		_dsp.drawPixel(x + i, line, col);
	}
}

void Screen::set_mode(uint8_t mode) {
	if (mode & 0x02)
		_dsp.clear();
	_mode4 = (mode & 0x08);

	bool screen2 = _screen2;
	_screen2 = (mode & 0x80);
	if (screen2 != _screen2) redraw();
}

void Screen::begin() {
	_ram.begin();
	_dsp.begin(BG_COLOUR, FG_COLOUR, ORIENT);
}

void Screen::checkpoint(Checkpoint &c) {
	_ram.checkpoint(c);
	c.write(_mode4);
	c.write(_screen2);
}

void Screen::restore(Checkpoint &c) {
	_ram.restore(c);
	c.read(_mode4);
	c.read(_screen2);
	redraw();
}
