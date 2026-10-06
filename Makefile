CORE ?= esp32
BOARD ?= esp32-sbc-fabgl

UploadSpeed := 921600
SERIAL_PORT ?= /dev/ttyUSB0
TERMINAL_SPEED := 115200
TERMINAL_EXTRA_FLAGS := -C serialout.txt
CPPFLAGS := -O3 -DTERMINAL_SPEED=$(TERMINAL_SPEED) -DVGA_RESOLUTION=MODE400x300 -DVGA_BIT_DEPTH=3
LIBRARIES := Adafruit_GFX Adafruit_BusIO Wire SPI ESP32Lib
PSRAM := enabled

include $(CORE).mk

%.h: roms/%.rom
	~/src/emul8/util/makerom -bp $< $* > $@

realclean: clean
	rm -f serialout.txt
