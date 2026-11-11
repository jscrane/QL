#!/usr/bin/env python3
"""Run the QL's IPC (Intel 8049) firmware and record what its speaker pin does.

Purpose: ground truth for the QL sound emulation. We load the real IPC firmware,
send it an "initiate sound" command and record the transitions of the speaker
pin (P2.1) against emulated time.

The ZX8302 serial link is not modelled at pin level. Instead the two things the
firmware does with it are replaced:
  * comdata (P2.7) reads low while a command or parameter nibble is waiting;
  * the firmware's "get 4 bits from ZX8302" routine ($074F) is intercepted and
    returns the next queued nibble.
Everything else (timer, interrupts, ports, the sound code) is the real firmware.

ROM source: either a binary image, or the annotated listing / Markdown made by
ipc8049_to_md.py (the bytes are rebuilt from it).

usage:
  ipc8049_sim.py selftest   LISTING_OR_ROM
  ipc8049_sim.py sound      LISTING_OR_ROM PITCH1 PITCH2 INTERVAL DURATION STEP WRAP RANDOM FUZZY [ms]
"""

import re
import sys

CRYSTAL_HZ = 11_000_000
CYCLE_US = 15 * 1e6 / CRYSTAL_HZ           # one machine cycle = 15 clocks
TICK_CYCLES = 32                            # timer prescaler


# ---------------------------------------------------------------- ROM loading

LISTING_LINE = re.compile(r"^([0-9A-F]{4}): ")


def parse_listing(path):
    """Return (rom bytearray(2048), rows) where rows = [(addr, bytes, mnemonic_text)]."""
    rom = bytearray(2048)
    rows = []
    for line in open(path, encoding="ascii", errors="replace"):
        line = line.rstrip("\n")
        m = LISTING_LINE.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        bs = [int(t, 16) for t in line[6:14].split()]
        text = line[14:].strip()
        for i, b in enumerate(bs):
            rom[addr + i] = b
        rows.append((addr, bs, text))
    return rom, rows


def load_rom(path):
    data = open(path, "rb").read()
    if data[:1] != b"" and (b"IPC" in data[:200] or LISTING_LINE.search(data[:4000].decode("ascii", "replace"))):
        rom, _ = parse_listing(path)
        return rom
    rom = bytearray(2048)
    rom[:len(data[:2048])] = data[:2048]
    return rom


# ------------------------------------------------------------- opcode metadata

_EXACT = {
    0x00: ("NOP", 1), 0x02: ("OUTL", 1), 0x03: ("ADD", 2), 0x05: ("EN", 1), 0x07: ("DEC", 1),
    0x08: ("INS", 1), 0x09: ("IN", 1), 0x0A: ("IN", 1), 0x13: ("ADDC", 2), 0x15: ("DIS", 1),
    0x16: ("JTF", 2), 0x17: ("INC", 1), 0x23: ("MOV", 2), 0x25: ("EN", 1), 0x26: ("JNT0", 2),
    0x27: ("CLR", 1), 0x35: ("DIS", 1), 0x36: ("JT0", 2), 0x37: ("CPL", 1), 0x39: ("OUTL", 1),
    0x3A: ("OUTL", 1), 0x42: ("MOV", 1), 0x43: ("ORL", 2), 0x45: ("STRT", 1), 0x46: ("JNT1", 2),
    0x47: ("SWAP", 1), 0x53: ("ANL", 2), 0x55: ("STRT", 1), 0x56: ("JT1", 2), 0x57: ("DA", 1),
    0x62: ("MOV", 1), 0x65: ("STOP", 1), 0x67: ("RRC", 1), 0x75: ("ENT0", 1), 0x76: ("JF1", 2),
    0x77: ("RR", 1), 0x83: ("RET", 1), 0x85: ("CLR", 1), 0x86: ("JNI", 2), 0x88: ("ORL", 2),
    0x89: ("ORL", 2), 0x8A: ("ORL", 2), 0x93: ("RETR", 1), 0x95: ("CPL", 1), 0x96: ("JNZ", 2),
    0x97: ("CLR", 1), 0x98: ("ANL", 2), 0x99: ("ANL", 2), 0x9A: ("ANL", 2), 0xA3: ("MOVP", 1),
    0xA5: ("CLR", 1), 0xA7: ("CPL", 1), 0xB3: ("JMPP", 1), 0xB5: ("CPL", 1), 0xB6: ("JF0", 2),
    0xC5: ("SEL", 1), 0xC6: ("JZ", 2), 0xC7: ("MOV", 1), 0xD3: ("XRL", 2), 0xD5: ("SEL", 1),
    0xD7: ("MOV", 1), 0xE3: ("MOVP3", 1), 0xE5: ("SEL", 1), 0xE6: ("JNC", 2), 0xE7: ("RL", 1),
    0xF5: ("SEL", 1), 0xF6: ("JC", 2), 0xF7: ("RLC", 1),
}


def info(op):
    """(mnemonic root, length) or None if the opcode is undefined."""
    if op in _EXACT:
        return _EXACT[op]
    if op & 0x1F == 0x04:
        return ("JMP", 2)
    if op & 0x1F == 0x14:
        return ("CALL", 2)
    if op & 0x1F == 0x12:
        return ("JB", 2)
    hi, lo = op >> 4, op & 0x0F
    if hi == 0 and lo >= 0x0C:
        return ("MOVD", 1)
    if hi == 3 and lo >= 0x0C:
        return ("MOVD", 1)
    if hi == 8 and lo >= 0x0C:
        return ("ORLD", 1)
    if hi == 9 and lo >= 0x0C:
        return ("ANLD", 1)
    table = {
        1: ("INC", 1), 2: ("XCH", 1), 3: ("XCHD", 1), 4: ("ORL", 1), 5: ("ANL", 1), 6: ("ADD", 1),
        7: ("ADDC", 1), 8: ("MOVX", 1), 9: ("MOVX", 1), 0xA: ("MOV", 1), 0xB: ("MOV", 2),
        0xC: ("DEC", 1), 0xD: ("XRL", 1), 0xE: ("DJNZ", 2), 0xF: ("MOV", 1),
    }
    if hi in table and (lo in (0, 1) or lo >= 8):
        if hi == 3 and lo in (0, 1):
            return ("XCHD", 1)
        if hi in (1, 2, 4, 5, 6, 7, 0xC, 0xD, 0xF) or lo in (0, 1) or lo >= 8:
            if hi in (8, 9) and lo >= 8:
                return None
            if hi == 0xA and lo == 0xC:
                pass
            return table[hi]
    return None


# ------------------------------------------------------------------------ CPU

class Link:
    """Stands in for the ZX8302 serial link: a queue of nibbles for the IPC to read."""

    def __init__(self):
        self.nibbles = []

    def send_command(self, cmd, params=()):
        self.nibbles.append(cmd & 0xF)
        for b in params:
            self.nibbles.append(b >> 4)
            self.nibbles.append(b & 0xF)

    @property
    def pending(self):
        return bool(self.nibbles)


class MCS48:
    def __init__(self, rom, link, prescaler_clear_on="strt"):
        self.rom = rom
        self.link = link
        self.prescaler_clear_on = prescaler_clear_on
        self.ram = [0] * 128
        self.pc = 0
        self.a = 0
        self.cy = self.ac = self.f0 = self.f1 = self.bs = 0
        self.sp = 0
        self.t = 0
        self.prescaler = 0
        self.timer_mode = 0                 # 0 stopped, 1 timer, 2 counter
        self.tf = 0
        self.tcnti = False
        self.in_int = False
        self.p1 = 0xFF
        self.p2 = 0xFF
        self.cycles = 0
        self.speaker = []                   # (cycle, level) for each change of P2.1
        self.sound_off_cycle = None
        self.reloads = []                   # (cycle, pitchx, timer count) at each sound timer reload
        self._sound_was_on = False

    # -- helpers
    def R(self, n):
        return self.ram[(24 if self.bs else 0) + n]

    def setR(self, n, v):
        self.ram[(24 if self.bs else 0) + n] = v & 0xFF

    def ind(self, i):
        return self.R(i) & 0x7F

    def psw(self):
        return (self.cy << 7) | (self.ac << 6) | (self.f0 << 5) | (self.bs << 4) | 0x08 | self.sp

    def set_psw(self, v):
        self.cy, self.ac, self.f0, self.bs, self.sp = (v >> 7) & 1, (v >> 6) & 1, (v >> 5) & 1, (v >> 4) & 1, v & 7

    def push(self):
        base = 8 + 2 * self.sp
        self.ram[base] = self.pc & 0xFF
        self.ram[base + 1] = ((self.pc >> 8) & 0x0F) | (self.psw() & 0xF0)
        self.sp = (self.sp + 1) & 7

    def pop(self, restore_psw):
        self.sp = (self.sp - 1) & 7
        base = 8 + 2 * self.sp
        self.pc = ((self.ram[base + 1] & 0x0F) << 8) | self.ram[base]
        if restore_psw:
            hi = self.ram[base + 1] & 0xF0
            self.cy, self.ac, self.f0, self.bs = (hi >> 7) & 1, (hi >> 6) & 1, (hi >> 5) & 1, (hi >> 4) & 1
            self.in_int = False

    def fetch(self):
        v = self.rom[self.pc & 0x7FF]
        self.pc = (self.pc + 1) & 0xFFF
        return v

    def write_port(self, port, value):
        if port == 2:
            old = self.p2
            self.p2 = value & 0xFF
            if (old ^ self.p2) & 0x02:
                self.speaker.append((self.cycles, (self.p2 >> 1) & 1))
        else:
            self.p1 = value & 0xFF

    def read_port(self, port):
        if port == 2:
            v = self.p2
            v = (v & 0x7F) | (0x00 if self.link.pending else 0x80)   # comdata low = data waiting
            return v
        return self.p1

    # -- time
    def add_time(self, n):
        self.cycles += n
        if self.timer_mode == 1:
            self.prescaler += n
            while self.prescaler >= TICK_CYCLES:
                self.prescaler -= TICK_CYCLES
                self.t = (self.t + 1) & 0xFF
                if self.t == 0:
                    self.tf = 1

    # -- run
    def step(self):
        if self.tf and self.tcnti and not self.in_int:
            self.tf = 0
            self.push()
            self.pc = 7
            self.in_int = True
            self.add_time(2)
            return
        # intercept "get 4 bits from ZX8302" ($074F)
        if self.pc == 0x074F and self.link.pending:
            v = self.link.nibbles.pop(0)
            self.a = v
            self.setR(6, v)
            self.setR(7, 0)
            self.pop(True)
            self.add_time(60)               # the real handshake takes a while; value is arbitrary
            return
        if self.pc == 0x040D:
            self.reloads.append((self.cycles, self.ram[0x42], self.ram[0x44]))
        op = self.fetch()
        inf = info(op)
        if inf is None:
            raise RuntimeError(f"undefined opcode {op:02X} at {(self.pc - 1):04X}")
        cycles = self.execute(op)
        self.add_time(cycles)
        on = bool(self.ram[0x20] & 2)
        if self._sound_was_on and not on:
            self.sound_off_cycle = self.cycles
        self._sound_was_on = on

    def run_cycles(self, n):
        end = self.cycles + n
        while self.cycles < end:
            self.step()

    # -- execute one instruction (opcode already fetched); returns machine cycles
    def execute(self, op):
        a = self.a
        lo = op & 0x07
        reg = op & 0x07
        two = 2

        def jump_cond(cond):
            off = self.fetch()
            if cond:
                self.pc = ((self.pc - 1) & 0xF00) | off
            return two

        def alu_add(x, carry):
            s = self.a + x + carry
            self.ac = 1 if ((self.a & 0xF) + (x & 0xF) + carry) > 0xF else 0
            self.cy = 1 if s > 0xFF else 0
            self.a = s & 0xFF

        # --- control flow
        if op & 0x1F == 0x04:                           # JMP
            addr = ((op & 0xE0) << 3) | self.fetch()
            self.pc = addr
            return 2
        if op & 0x1F == 0x14:                           # CALL
            addr = ((op & 0xE0) << 3) | self.fetch()
            self.push()
            self.pc = addr
            return 2
        if op & 0x1F == 0x12:                           # JBb
            return jump_cond((a >> (op >> 5)) & 1)
        if op == 0x83:
            self.pop(False)
            return 2
        if op == 0x93:
            self.pop(True)
            return 2
        if op == 0xB3:                                  # JMPP @A
            self.pc = (self.pc & 0xF00) | self.rom[((self.pc - 1) & 0xF00) | a]
            return 2
        if op == 0xA3:                                  # MOVP A,@A
            self.a = self.rom[(self.pc & 0xF00) | a]
            return 2
        if op == 0xE3:                                  # MOVP3 A,@A
            self.a = self.rom[0x300 | a]
            return 2
        if op == 0xC6:
            return jump_cond(a == 0)
        if op == 0x96:
            return jump_cond(a != 0)
        if op == 0xF6:
            return jump_cond(self.cy == 1)
        if op == 0xE6:
            return jump_cond(self.cy == 0)
        if op == 0x36:
            return jump_cond(True)                      # T0 idles high
        if op == 0x26:
            return jump_cond(False)
        if op == 0x56:
            return jump_cond(True)                      # T1 idles high
        if op == 0x46:
            return jump_cond(False)
        if op == 0x86:
            return jump_cond(False)                     # INT pin idles high, so JNI never jumps
        if op == 0xB6:
            return jump_cond(self.f0 == 1)
        if op == 0x76:
            return jump_cond(self.f1 == 1)
        if op == 0x16:
            t = self.tf
            self.tf = 0
            return jump_cond(t == 1)
        if 0xE8 <= op <= 0xEF:                          # DJNZ Rr
            v = (self.R(reg) - 1) & 0xFF
            self.setR(reg, v)
            return jump_cond(v != 0)

        # --- accumulator
        if op == 0x03:
            alu_add(self.fetch(), 0); return 2
        if op == 0x13:
            alu_add(self.fetch(), self.cy); return 2
        if op in (0x60, 0x61):
            alu_add(self.ram[self.ind(op & 1)], 0); return 1
        if op in (0x70, 0x71):
            alu_add(self.ram[self.ind(op & 1)], self.cy); return 1
        if 0x68 <= op <= 0x6F:
            alu_add(self.R(reg), 0); return 1
        if 0x78 <= op <= 0x7F:
            alu_add(self.R(reg), self.cy); return 1
        if op == 0x53:
            self.a = a & self.fetch(); return 2
        if op == 0x43:
            self.a = a | self.fetch(); return 2
        if op == 0xD3:
            self.a = a ^ self.fetch(); return 2
        if op in (0x50, 0x51):
            self.a = a & self.ram[self.ind(op & 1)]; return 1
        if op in (0x40, 0x41):
            self.a = a | self.ram[self.ind(op & 1)]; return 1
        if op in (0xD0, 0xD1):
            self.a = a ^ self.ram[self.ind(op & 1)]; return 1
        if 0x58 <= op <= 0x5F:
            self.a = a & self.R(reg); return 1
        if 0x48 <= op <= 0x4F:
            self.a = a | self.R(reg); return 1
        if 0xD8 <= op <= 0xDF:
            self.a = a ^ self.R(reg); return 1
        if op == 0x17:
            self.a = (a + 1) & 0xFF; return 1
        if op == 0x07:
            self.a = (a - 1) & 0xFF; return 1
        if op == 0x27:
            self.a = 0; return 1
        if op == 0x37:
            self.a = a ^ 0xFF; return 1
        if op == 0x47:
            self.a = ((a << 4) | (a >> 4)) & 0xFF; return 1
        if op == 0xE7:
            self.a = ((a << 1) | (a >> 7)) & 0xFF; return 1
        if op == 0x77:
            self.a = ((a >> 1) | (a << 7)) & 0xFF; return 1
        if op == 0xF7:
            c = self.cy; self.cy = a >> 7; self.a = ((a << 1) | c) & 0xFF; return 1
        if op == 0x67:
            c = self.cy; self.cy = a & 1; self.a = ((a >> 1) | (c << 7)) & 0xFF; return 1
        if op == 0x57:                                  # DA A
            if (a & 0x0F) > 9 or self.ac:
                a += 6
                if a > 0xFF:
                    self.cy = 1
                a &= 0xFF
            if (a >> 4) > 9 or self.cy:
                a += 0x60
                if a > 0xFF:
                    self.cy = 1
                a &= 0xFF
            self.a = a
            return 1
        if op == 0x97:
            self.cy = 0; return 1
        if op == 0xA7:
            self.cy ^= 1; return 1
        if op == 0x85:
            self.f0 = 0; return 1
        if op == 0x95:
            self.f0 ^= 1; return 1
        if op == 0xA5:
            self.f1 = 0; return 1
        if op == 0xB5:
            self.f1 ^= 1; return 1

        # --- moves
        if op == 0x23:
            self.a = self.fetch(); return 2
        if 0xF8 <= op <= 0xFF:
            self.a = self.R(reg); return 1
        if 0xA8 <= op <= 0xAF:
            self.setR(reg, a); return 1
        if 0xB8 <= op <= 0xBF:
            self.setR(reg, self.fetch()); return 2
        if op in (0xF0, 0xF1):
            self.a = self.ram[self.ind(op & 1)]; return 1
        if op in (0xA0, 0xA1):
            self.ram[self.ind(op & 1)] = a; return 1
        if op in (0xB0, 0xB1):
            self.ram[self.ind(op & 1)] = self.fetch(); return 2
        if 0x28 <= op <= 0x2F:
            r = self.R(reg); self.setR(reg, a); self.a = r; return 1
        if op in (0x20, 0x21):
            i = self.ind(op & 1); r = self.ram[i]; self.ram[i] = a; self.a = r; return 1
        if op in (0x30, 0x31):
            i = self.ind(op & 1); r = self.ram[i]
            self.ram[i] = (r & 0xF0) | (a & 0x0F); self.a = (a & 0xF0) | (r & 0x0F); return 1
        if 0x18 <= op <= 0x1F:
            self.setR(reg, self.R(reg) + 1); return 1
        if op in (0x10, 0x11):
            i = self.ind(op & 1); self.ram[i] = (self.ram[i] + 1) & 0xFF; return 1
        if 0xC8 <= op <= 0xCF:
            self.setR(reg, self.R(reg) - 1); return 1
        if op == 0xC7:
            self.a = self.psw(); return 1
        if op == 0xD7:
            self.set_psw(a); return 1
        if op == 0x42:
            self.a = self.t; return 1
        if op == 0x62:
            self.t = a
            if self.prescaler_clear_on == "mov_t":
                self.prescaler = 0
            return 1

        # --- timer / interrupt control
        if op == 0x55:
            self.timer_mode = 1
            if self.prescaler_clear_on == "strt":
                self.prescaler = 0
            return 1
        if op == 0x45:
            self.timer_mode = 2; return 1
        if op == 0x65:
            self.timer_mode = 0; return 1
        if op == 0x25:
            self.tcnti = True; return 1
        if op == 0x35:
            self.tcnti = False; self.tf = 0 if False else self.tf; return 1
        if op in (0x05, 0x15, 0x75):
            return 1
        if op == 0xC5:
            self.bs = 0; return 1
        if op == 0xD5:
            self.bs = 1; return 1
        if op in (0xE5, 0xF5):
            return 1
        if op == 0x00:
            return 1

        # --- ports
        if op == 0x09:
            self.a = self.read_port(1); return 2
        if op == 0x0A:
            self.a = self.read_port(2); return 2
        if op == 0x39:
            self.write_port(1, a); return 2
        if op == 0x3A:
            self.write_port(2, a); return 2
        if op == 0x89:
            self.write_port(1, self.p1 | self.fetch()); return 2
        if op == 0x8A:
            self.write_port(2, self.p2 | self.fetch()); return 2
        if op == 0x99:
            self.write_port(1, self.p1 & self.fetch()); return 2
        if op == 0x9A:
            self.write_port(2, self.p2 & self.fetch()); return 2
        if op == 0x08:
            self.a = 0xFF; return 2                     # INS A,BUS
        if op in (0x02, 0x88, 0x98):
            if op != 0x02:
                self.fetch()
            return 2
        if 0x0C <= op <= 0x0F:
            self.a = 0; return 2                        # MOVD A,Pp (expander absent)
        if 0x3C <= op <= 0x3F or 0x8C <= op <= 0x8F or 0x9C <= op <= 0x9F:
            return 2
        if op in (0x80, 0x81):
            self.a = 0; return 2                        # MOVX A,@Ri
        if op in (0x90, 0x91):
            return 2                                    # MOVX @Ri,A (WRN strobe)
        raise RuntimeError(f"unimplemented opcode {op:02X} at {(self.pc - 1):04X}")


# ------------------------------------------------------------------- commands

def selftest(path):
    rom, rows = parse_listing(path)
    bad = 0
    for addr, bs, text in rows:
        first = text.split()[0] if text.split() else ""
        if first == "DFB":
            continue
        inf = info(bs[0])
        if inf is None or inf[1] != len(bs) or (inf[0] != first and not (inf[0] == "JB" and first.startswith("JB"))):
            bad += 1
            print(f"MISMATCH {addr:04X}: bytes={bs} listing={text!r} mine={inf}")
    print(f"{len(rows)} listing lines checked, {bad} mismatches")
    return bad == 0


def run_sound(rom, p1, p2, interval, duration, step, wrap, rand, fuzzy, run_ms=500.0,
              prescaler_clear_on="strt", warmup_ms=60.0):
    link = Link()
    cpu = MCS48(rom, link, prescaler_clear_on)
    cpu.run_cycles(int(warmup_ms * 1000 / CYCLE_US))
    t0 = cpu.cycles
    n_before = len(cpu.speaker)
    params = [p1, p2, interval & 0xFF, interval >> 8, duration & 0xFF, duration >> 8,
              ((step & 0xF) << 4) | (wrap & 0xF), ((rand & 0xF) << 4) | (fuzzy & 0xF)]
    link.send_command(0xA, params)
    cpu.run_cycles(int(run_ms * 1000 / CYCLE_US))
    edges = [((c - t0) * CYCLE_US, lvl) for c, lvl in cpu.speaker[n_before:]]
    off = None if cpu.sound_off_cycle is None or cpu.sound_off_cycle < t0 else (cpu.sound_off_cycle - t0) * CYCLE_US
    return edges, off, cpu


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    cmd, path = argv[1], argv[2]
    if cmd == "selftest":
        return 0 if selftest(path) else 1
    if cmd == "sound":
        rom = load_rom(path)
        vals = [int(x, 0) for x in argv[3:11]]
        ms = float(argv[11]) if len(argv) > 11 else 500.0
        edges, off, cpu = run_sound(rom, *vals, run_ms=ms)
        print(f"{len(edges)} speaker edges; sound-on flag cleared at {off if off is None else round(off)} us")
        for i in range(min(len(edges), 12)):
            print(f"  edge {i}: t={edges[i][0]:10.1f} us level={edges[i][1]}")
        if len(edges) > 2:
            gaps = [edges[i + 1][0] - edges[i][0] for i in range(len(edges) - 1)]
            print(f"  half-period: first={gaps[0]:.1f} us  median={sorted(gaps)[len(gaps)//2]:.1f} us")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
