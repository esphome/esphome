# crc32 helper and the signed OTA block check (esphome#22398, 2026-11-07)

`scripts/crc32/crctest.hpp` checks `esphome::crc32()` against Python `zlib.crc32` values: `"123456789"` = `0xCBF43926`,
empty input returns the seed, chained calls (4 + 5, 300 one-byte calls, 97-byte chunks from an odd offset), bytes
0..255, 32 × 0xFF, 4093 bytes from offsets 0 to 3, and 8 KiB timed with `(esphome::micros)()` (best of five).
`common-crc.yaml` runs it at boot and every 20 s and also makes an unqualified `crc32(...)` call from a lambda, which
catches a clash with anything the generated `main.cpp` pulls in. Paths and timings seen (8 KiB): ESP32 ROM 512 µs,
ESP8266 PROGMEM nybble table 3072 µs, RP2350 932 µs, BW15 (FlashDB) 656 µs, BW12 2000 µs, BK7238 1026 µs.

Signed OTA (the only converted caller, `block_is_valid()` in `ota_signature_esp_idf.cpp`), on an ESP32-C3
(`signed-ota-c3.yaml`, `signed_ota_verification: signing_scheme: rsa3072, verification_keys: [keyA.pem]`):
1. `python -m espsecure generate-signing-key --version 2 --scheme rsa3072 keyA.pem` (and keyB).
2. Build four tagged images with `esphome -s tag <name> compile`, serial-flash the first factory image (no on-boot
   signature check without secure boot), copy each `firmware.ota.bin`.
3. `sign_variants.py <image> keyA.pem keyB.pem <prefix>` writes key A, key B, B then A, stored CRC flipped, byte 2
   changed (covered only by the CRC), the same with the CRC recomputed by zlib, and B then A with block 1's CRC flipped.
4. `otaseq.sh` sends them with `esphome upload --file`; send the rejects first so every check runs in the same build,
   then the accepted ones (each boots and logs its `BUILDTAG`).
Expected: unsigned, B only and every CRC edit rejected; A, B+A and the zlib-fixed CRC accepted. The byte 2 pair isolates
the CRC from the signature and key digest.

Gotchas: an ESP-IDF image can contain `E7 02` at a 4 KiB boundary by chance; find the signature sector by the CRC, as
`sign_variants.py` does. An unsigned image sent after a signed one can be rejected with "trusted key failed to verify"
instead of "no signature block": the partition still holds the old signature sector past the new image's end.

Results: https://github.com/esphome/esphome/pull/22398#issuecomment-6716330334 and #issuecomment-6716545831.
