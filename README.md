# QL

An emulation of the Sinclair QL with 640kB of RAM and two double-density CST QDisc floppies.

So far, this has only been tested on an Olimex `esp32-sbc-fabgl` [board](https://www.olimex.com/Products/Retro-Computers/ESP32-SBC-FabGL/open-source-hardware).

## Documentation

The `docs` directory contains the `ipc8049` rom downloaded by Marcel Kilgus and converted to markdown.
(The original is [here](https://www.sinclairql.net/repository.html#Firmware).)

It also contains a python emulation of the 8049 (written by Claude) which might be useful in future.

Claude also wrote the sound emulation in `beep_model.{cpp,h}`, and the `tests/` directory.

## ROMs

The `roms/` directory contains some original images downloaded from [here](https://sinclairql.net/djw/qlrom/index.html#qlrom).

- `cst_qdisc_116.rom`: CST QDisc version 1.16 (this is the emulated version)
- `cst_qdisc_118.rom`: CST QDisc version 1.18 (unscrambled: the original used a non-standard address mapping which made disassembly difficult)
- `js.rom`: QDOS rom version `JS`
- `tkii220.rom`: QJUMP Toolkit II, version 2.20 ([here](https://www.lingula.org.uk/~steve/share/ToolKit-II-Manual.pdf) is the manual for version 2.13)

## Function Keys

`F1` to `F5` are taken by the QL itself.

The emulator uses:
- `F6` to reset the machine
- `F7` to advance to next image
- `F8` to rewind to first image
- `F9` to select device for image
