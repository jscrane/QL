CORE ?= esp32
BOARD ?= esp32-sbc-fabgl

UploadSpeed := 921600
SERIAL_PORT ?= /dev/ttyUSB0
TERMINAL_SPEED := 115200
TERMINAL_EXTRA_FLAGS := -C serialout.txt
CPPFLAGS := -Wall -O3 -DTERMINAL_SPEED=$(TERMINAL_SPEED) -DVGA_RESOLUTION='Mode(0, 24, 40, 552, 3, 3, 18, 576, 2, 20800000, 0, 0)' -DVGA_BIT_DEPTH=3 -DUSE_SD
#CPPFLAGS += -DUSE_PS2_KBD -DPS2_KEYMAP=\"UK\"
LIBRARIES := Adafruit_GFX Adafruit_BusIO Wire SPI ESP32Lib FS SD
#LIBRARIES += PS2KeyAdvanced PS2KeyMap
PSRAM := enabled

include $(CORE).mk

%.h: roms/%.rom
	~/src/emul8/util/makerom -bp $< $* > $@

realclean: clean
	rm -f serialout.txt
