ROMS := $(wildcard *.rom)
HEADERS := $(ROMS:.rom=.h)

all: $(HEADERS)

%.h: %.rom
	makerom -bp $< $* > $@
