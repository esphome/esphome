# Heap allocation counts and callers (#22509, #22510, ratgdo/esphome-ratgdo#705)

## Counting

Replace the global `operator new` in an `esphome: includes:` header on ESP32 (Arduino or ESP-IDF) and log the
delta from an `interval`. Compare a PR build with its base on the same flow; the rate per second shows churn.
- The ESP8266 core defines `operator new` non-weak. `-Wl,--wrap=_Znwj` worked in a sensor-only config but
  boot-looped a neopixelbus config (alignment fault in the wrapper), so report free heap only there.
- `vec = {}` keeps a std::vector's capacity (it is an initializer_list assignment); only
  `std::vector<T>().swap(vec)` or `clear()` plus `shrink_to_fit()` frees it. Seen as heap that did not come back
  after a WLED effect stopped.

## Callers

`scripts/heap_alloc/`:
- `allocring.hpp` records the size, time and five caller PCs (`esp_backtrace_get_next_frame`) of every
  `operator new` into a 1024 entry ring (about 28 KB of RAM) once armed.
- `allocring.yaml` (package, substitution `alloc_arm_s`) arms it after that many seconds of uptime and adds a
  "Dump allocations" button that logs the ring.
- Lambdas call `alloc_marker(a, b)` to write a size 0 entry, for example on each cover operation change, so the
  dump splits into phases.
- `decode.py LOG ELF [ADDR2LINE]` prints each entry with its callers as function and file:line. The ESP-IDF ELF is
  `.esphome/build/<name>/build/firmware.elf`; any xtensa addr2line reads an ESP32 ELF.
- `cycles_cover.py HOST N OUT` opens and closes the first cover N times (`cover_command(position=...)`, waiting for
  idle each way) with a states-only API client, then subscribes to logs and presses the dump button.

Findings:
- Arm well after boot. The scheduler trims its item pool and shrinks its `to_add_` and `items_` vectors 10 s after
  setup (`SCHEDULER_FREELIST_TRIM_DELAY_MS`), so the first burst of timers after that regrows them once.
- Repeat the action several times in one session and count only what comes back. On a ratgdo cover (an external
  component, ESP32 ESP-IDF) the first door open after boot allocated 18 times: four 36 B SchedulerItems,
  `to_add_` and `items_` growth, and the API `DeferredBatch` vector. Cycles 2 and 3 allocated nothing.
- An API client that connects allocates about 15 times (connection object, list entities and initial state
  batches). Connect before the run, subscribe to states only, and subscribe to logs only for the dump.
