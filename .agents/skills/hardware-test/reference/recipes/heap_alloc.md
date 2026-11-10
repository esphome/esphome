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
- `allocring.hpp` counts every `operator new` and, once armed, keeps the first 1024 with their size, time and
  five caller PCs, about 28 KB of RAM. It disarms itself when full. The frame walker
  (`esp_backtrace_get_next_frame`) exists only on Xtensa (ESP32, S2, S3); on RISC-V chips entries keep size and
  time without callers. Aligned `operator new` (`std::align_val_t`) is not replaced, so it is not counted;
  nothrow `new` goes through the replaced one.
- `allocring.yaml` (package, substitution `alloc_arm_s`) arms it that many seconds after boot and adds a
  "Dump allocations" button that logs what was kept.
- `alloc_marker(a, b)` from a lambda writes a marker entry, for example on each cover operation change, so the
  dump splits into phases.
- `dump.py HOST OUT` presses the button and saves the dump, pressing again when the log subscription started
  late. `decode.py LOG ELF [ADDR2LINE]` prints each entry with its callers as function and file:line, and exits 2
  when entries are missing or repeated or the buffer filled. It reads only the last whole dump in the file. The ESP-IDF ELF is `.esphome/build/<name>/build/firmware.elf`.
- `cycles_cover.py HOST N OUT` opens and closes the first cover N times (`cover_command(position=...)`, waiting
  for idle each way) with a states-only API client, then saves the dump the same way. It exits 2 when a move
  never reached its position.

Findings:
- Arm well after boot: the scheduler regrows its storage once after the freelist trim (see `scheduler.md`).
- Repeat the action several times in one session and count only what comes back. On a ratgdo cover (an external
  component, ESP32 ESP-IDF) the first door open after boot allocated 18 times: four 36 B SchedulerItems,
  `to_add_` and `items_` growth, and the API `DeferredBatch` vector. Cycles 2 and 3 allocated nothing.
- Keep the API client out of the count (see `api.md`): connect before the run, subscribe to states only, and
  subscribe to logs only for the dump.
