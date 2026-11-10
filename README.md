# QL

An emulation of the Sinclair QL with 256kB of RAM and two double-density CST QDisc floppies.

So far, this has only been tested on an Olimex `esp32-sbc-fabgl` board.

## Documentation

The `docs` directory contains the `ipc8049` rom downloaded by Marcel Kilgus and converted to markdown.
(The original is [here](https://www.sinclairql.net/repository.html#Firmware).)

It also contains a python emulation of the 8049 (written by Claude) which might be useful in future.

Claude also wrote most of the sound emulation in `snd_adapter.{cpp,h}`, and the `tests/` directory.

## ROMs

The `roms/` directory contains some original images downloaded from [here](https://sinclairql.net/djw/qlrom/index.html#qlrom).

- `cst_qdisc_116.rom`: CST QDisc version 1.16 (this is the emulated version)
- `cst_qdisc_118.rom`: CST QDisc version 1.18 (unscrambled: the original had a weird address mapping which caused it to appear scrambled)
- `js.rom`: QDOS rom version `JS`
- `tkii220.rom`: QJUMP Toolkit II, version 2.20

## Function Keys

`F1` to `F5` are used by the QL itself.

- `F6` resets the machine
- `F7` advance to next image
- `F8` rewind to first image
- `F9` select device for image
