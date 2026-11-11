CORE ?= esp32
BOARD ?= esp32-sbc-fabgl

UploadSpeed := 921600
SERIAL_PORT ?= /dev/ttyUSB0
TERMINAL_SPEED := 115200
TERMINAL_EXTRA_FLAGS := -C serialout.txt
CPPFLAGS := -Wall -O3 -DTERMINAL_SPEED=$(TERMINAL_SPEED) -DVGA_RESOLUTION=VGA_552x288 -DVGA_BIT_DEPTH=3 -DUSE_SD -DMEMORY_ADDRESS_WIDTH=20 -DUSE_HOST_KBD
#CPPFLAGS += -DUSE_PS2_KBD -DPS2_KEYMAP=\"UK\"
#CPPFLAGS += -DDEBUGGING=0x22
LIBRARIES := Adafruit_GFX Adafruit_BusIO Wire SPI ESP32Lib FS SD
#LIBRARIES += PS2KeyAdvanced PS2KeyMap
PSRAM := enabled

QL: rom-headers all

rom-headers:
	$(MAKE) -C roms

include $(CORE).mk

realclean: clean
	rm -f serialout.txt
