# crc32 helper and the signed OTA block check (esphome#22398, 2026-11-07)

`scripts/crc32/crctest.hpp` checks `esphome::crc32()` against Python `zlib.crc32` values: `"123456789"` = `0xCBF43926`,
empty input returns the seed, chained calls (4 + 5, 300 one-byte calls, 97-byte chunks from an odd offset), bytes
0..255, 32 × 0xFF, 4093 bytes from offsets 0 to 3, and 8 KiB timed with `(esphome::micros)()` (best of five).
`common-crc.yaml` runs it at boot and every 20 s and also makes an unqualified `crc32(...)` call from a lambda, which
catches a clash with anything the generated `main.cpp` pulls in. Paths and timings seen (8 KiB): ESP32 ROM 512 µs,
ESP8266 PROGMEM nybble table 3072 µs, RP2350 932 µs, BW15 (FlashDB) 656 µs, BW12 2000 µs, BK7238 1026 µs.

The signed OTA signature block check (`block_is_valid()`), the PR's only converted caller, ran the accept and reject
cases in `ota.md` (signed OTA with several trusted keys).

Results: https://github.com/esphome/esphome/pull/22398#issuecomment-6716330334 and #issuecomment-6716545831.
