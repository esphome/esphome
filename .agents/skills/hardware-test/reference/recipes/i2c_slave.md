# Fake I2C slave (#21586)

`scripts/i2c_slave`: a D1 mini running an Arduino `Wire` slave at 0x40 (`i2cslave.yaml` with `i2cslave.hpp`): a fake
HDC1080 plus a 256 byte register file, and reads of 0x7E and 0x7F stretch the clock for 0.5 and 5 ms. It needs
`libraries: [Wire]` and 160 MHz. With jumper wires only and the internal pull ups it works to about 50 kHz; it cannot
keep up with a true 100 kHz hardware master.

The master (`i2cbk.yaml`, a BK7238 on P26/P24, needs the software I2C bus from that PR) writes a counter, reads it
back with a repeated start, probes an absent address and reads both stretch registers every 2 s, logging `RW OK`
(the read back matches, the absent address NACKs and both stretched reads succeed) or `RW BAD` with running totals.
Control: the same blocks on a classic ESP32 devkit (SDA GPIO21, SCL GPIO22) with hardware I2C at 25 and 50 kHz got
every repeated start read right, so failures on a bit banged master at a similar real clock point at the master.
On the NiceMCU WB3S V1 board (BK7238), P26 and P24 are on the right-hand header.

Fake devices that need no real chip (in-firmware buses, the Pico 2 W target, the DS2482-800 simulator, NACK
injection) are in [fake_i2c.md](fake_i2c.md).

## I2C wire A/B of a bus change (#22780, write_prefixed)

`scripts/i2c_wire_ab/`.
- Host (`host/run.sh TREE OUT`, then `host/compare.py BASE PR`): copies a harness into the tree's
  `tests/components/i2c/` (removed again afterwards) and runs `script/cpp_unit_test.py i2c`. `extract.py` cuts the
  Arduino and ESP-IDF `write_readv`/`transfer_` bodies out of the tree and compiles them against the tree's real headers
  with stub `Wire.h` (esp8266 core 3.1.2 and arduino-pico 6.3.2 buffer rules, 128 and 256 bytes) and a stub
  `driver/i2c_master.h` (IDF 5.5.5 checks: at most SOC_I2C_CMD_REG_NUM jobs, NACK before STOP); the software bus is
  compiled whole with `#define InternalGPIOPin wab::FakePin` over a bit-level target. Every bus emits one token stream
  (`S 90+ 02+ ... P`, `Sr`, `rA0+`), with mallocs counted through macOS `malloc_logger` (ASan is dropped on macOS).
  Stub headers must live outside the test dir (a subfolder there is taken as a platform domain) and reach the build
  through a `benchmark.yaml` `esphome: build_flags: [-I...]`; `.h` files in the test dir are included into main.cpp, so
  generated bodies are `.inc`. Explicit-instantiation member robbers must be declared at global scope. The sources are
  stored as `.cpp.example`, `.h.example` and `.hpp` so the repo's C++ lints skip them; `run.sh` copies them back.
- Device: `dev/snf.hpp` is a GPIO interrupt sniffer on the I2C pads (an ESP32-C2 with an ADS1015: SDA 4, SCL 5,
  `dev/c2common.yaml`) plus the same case sweep and `-Wl,--wrap=malloc`; at 10 to 20 kHz on the C2 it still drops a bit
  in about 0.5 % of transactions (mostly at the chip's NACK), so capture three runs per build and judge with
  `clean.py`/`maj.py`. On the software bus it is exact. `dev/pdslave.hpp` + `snfpd.hpp` (`pdcommon.yaml`) turn an
  ESP32-PICO-D4 into its own target at 0x48 on GPIO32/33 (ACKs every byte, so 300-byte writes reach the wire): exact on
  6 of 6 captures at 25 kHz once one warm-up probe runs before the sweep (the ISR runs from flash; IRAM_ATTR on header
  inline functions fails to link with l32r literal errors). `mksw.py` turns a tree's LibreTiny-only software bus into
  a header so an ESP32 build can run it on real pins. The ADS1015 NACKs the third byte after the pointer, so long
  writes on that chip end there on every build.
- `ard/swp.hpp` is the same sweep for ESP8266 (GPIO interrupt sniffer on GPIO4/5), RP2 (raw IO_IRQ_BANK0 handler) and
  BK72xx (no sniffer), with `ard/common.yaml` plus a board file (`d1.yaml`, `pico.yaml`, `bk.yaml`). Heap counters per
  platform: ESP8266 `-Wl,--wrap=malloc` with `__wrap_malloc` marked IRAM_ATTR (in flash it crash-loops with
  IllegalInst at the wrapper's first instruction); arduino-pico already wraps malloc and `_malloc_r` (a second wrap is
  a duplicate symbol), so wrap `__malloc_lock` (malloc and free each count once); LibreTiny already wraps malloc into
  pvPortMalloc, so wrap `pvPortMalloc`. Each image checks itself with `ALLOC_PROBE 1`.
- Facts: on a D1 mini without a target, register writes up to 128 bytes total show `S 90- P`, longer ones fail with
  rc 6 and nothing on the wire; a write+read whose address is NACKed leaves no STOP (`S 90-`), dev too.
  arduino-pico: after a register write over 256 bytes, the next empty write (a probe) sends the stale buffer to the
  failed write's address.
