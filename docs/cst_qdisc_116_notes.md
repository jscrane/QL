## CST QDisc ROM — disassembly notes

### Hardware register mapping

`A4` is initialised by `L3` to `$CFFF0`, the base of the WD1770 register block.

| Address            | Register         | Access                                                      |
| ------------------ | ---------------- | ----------------------------------------------------------- |
| `$CFFF0` (`0(A4)`) | Status / Command | Read status; write command                                  |
| `$CFFF1` (`1(A4)`) | Track            | Read/write track register                                   |
| `$CFFF2` (`2(A4)`) | Sector           | Read/write sector register                                  |
| `$CFFF3` (`3(A4)`) | Data             | Read/write data register                                    |
| `$CFFF8`           | QDisc latch      | Additional controller register; purpose not yet established |

WD1770 register selection uses A1/A0: `00` = status/command, `01` = track, `10` = sector, `11` = data.

### `L3` — initialise register base

```asm
00000178 L3  LEA $3FF0(PC),A4
0000017C     RTS
```

Sets `A4 = $CFFF0`, the WD1770 register base.

### `L290`–`L294` — data-transfer polling

These routines poll the WD1770 status register and transfer bytes through the data register.

* `L290`–`L291`: poll status bits 0–1; if the expected condition doesn't occur, loop with a timeout counter.
* `L292`: similar polling loop; when the condition is met, read one byte from `(A2)` into `(A1)+`.
* `L293`: another status-polling loop that reads a byte from `(A2)`.
* `L294`: decrements a counter while repeating `L293`.

In the observed code, `A2` is set to `A4+3`, so `(A2)` accesses the WD1770 data register.

The polling logic masks status with `#3`. A masked value of `0` branches to `L299`; a value of `1` loops; values `2` and `3` allow the data-read path to proceed.

### `L299`–`L303` — wait for command completion / return status

```asm
L299 MOVEQ #56,D0
L300 BTST.B #0,(A4)
    BEQ L301
    DBRA D0,L300
    BRA L277
L301 ADDQ.W #1,D1
    BNE L303
L302 MOVEQ #92,D0
    AND.B (A4),D0
L303 RTS
```

* `L300` polls status bit 0 (BUSY), waiting for it to clear.
* If BUSY doesn't clear within the loop, control returns to `L277`.
* `L301` increments `D1`.
* If `D1` wraps to zero, `L302` masks status with `$5C` before returning.

### `L305`–`L306` — issue a WD1770 command and poll BUSY

```asm
L305 MOVE.B D0,(A4)
L306 MOVEQ #8,D0
    BTST.B #0,(A4)
    DBNE D0,L306
    ROR.L #8,D0
    RTS
```

`L305` writes the command to the WD1770 command register. `L306` polls BUSY, using a bounded loop.

### `L307`–`L314` — periodic drive-state poller

`L307` is installed as the ROM's 50/60 Hz polling routine.

* `L307` tests status bit 7, treated by this code as the drive's NOT READY indication.
* If bit 7 is clear, branch to `L311` (ready path).
* If bit 7 is set, enter the not-ready path, updating driver state and countdown variables at offsets within `(A3)`.
* `L308`–`L310` continue handling those not-ready state/countdown cases.
* `L311`–`L313` handle the ready path, updating state and sometimes writing to `8(A4)` (QDisc register `$CFFF8`).
* `L314` writes `$D0` to the WD1770 command register, issuing FORCE INTERRUPT, and updates state byte `67(A3)`.

Observed emulator behaviour: returning status `$80` for an absent disk makes the poller take the not-ready path. 
The QDOS flashing cursor appeared much sooner after READ SECTOR commands were changed to complete immediately with NOT READY rather than remaining BUSY indefinitely.

### Observed WD1770 commands

From the emulator trace and current implementation:

* `$0B`: RESTORE command variant; the emulated status was set to indicate INDEX, then BUSY was cleared.
* `$5A`: STEP IN command.
* `$18`–`$1B`: SEEK command variants; the emulator approximates SEEK by copying the data register value into the track register.
* `$88`: READ SECTOR command.
* `$D0`: FORCE INTERRUPT.

The SEEK behaviour is currently a simplified emulation, not a complete simulation of head movement or seek timing.

### READ SECTOR with no disk

The ROM writes `$88` to the command register after setting track 1. The trace showed status `$03` initially and then `$01` indefinitely 
when the emulator left the controller BUSY. This caused the ROM's bounded polling loops to repeat.

When the emulator instead completes the command immediately with status `$80` (NOT READY, BUSY clear), the ROM proceeds and the flashing cursor appears much sooner.

The observed trace includes a read of the sector register returning `$00` before the READ SECTOR command. We have not yet established 
whether this is a valid sector number, an uninitialised register value, or a QDisc-specific convention.

### Still to establish

* Exact purpose and bit meanings of the QDisc latch at `$CFFF8`.
* Exact disk-image geometry and physical layout expected by QDisc.
* How the side-select latch maps onto image offsets.
* Sector numbering conventions and the expected initial filesystem sectors.
* Full WD1770 error/status behaviour for missing sectors, missing disks, and read errors.

