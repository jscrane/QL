ROMS := $(wildcard *.rom)
HEADERS := $(ROMS:.rom=.h)

all: $(HEADERS)

clean:
	rm -f $(HEADERS)

%.h: %.rom
	makerom -bp $< $* > $@
