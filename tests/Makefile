R65EMU  ?= ../../libraries/r65emu
CPPFLAGS = -I$(R65EMU)/src -I$(R65EMU)/tests -I. -I.. -DDEBUGGING=0x01
CXXFLAGS = -g -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS  = -fsanitize=address,undefined
vpath %.cpp .. $(R65EMU)/src

TESTS := snd_adapter_tests ipc_tests

all: $(TESTS)

snd_adapter_tests: snd_adapter_tests.o snd_adapter.o
ipc_tests: ipc_tests.o ipc.o snd_adapter.o kbd_adapter.o

$(TESTS):
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -c -o $@ $<

check: all
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done

clean:
	rm -f $(TESTS) *.o *.d

-include *.d
