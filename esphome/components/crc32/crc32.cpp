#include "crc32.h"
#ifdef USE_CRC32

#ifdef USE_ESP32
#include "esp_rom_crc.h"
#endif

namespace esphome::crc32 {

#ifdef USE_ESP32

// The lookup table and loop live in the mask ROM
uint32_t crc32(uint32_t crc, const uint8_t *data, size_t len) { return esp_rom_crc32_le(crc, data, len); }

#else

// Nibble-at-a-time software implementation: 64 bytes of table instead of the
// usual 1 KiB, at roughly half the byte-table speed
static constexpr uint32_t CRC32_NIBBLE_TABLE[16] = {
    0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
    0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C,
};

uint32_t crc32(uint32_t crc, const uint8_t *data, size_t len) {
  crc = ~crc;
  while (len--) {
    crc ^= *data++;
    crc = (crc >> 4) ^ CRC32_NIBBLE_TABLE[crc & 0x0F];
    crc = (crc >> 4) ^ CRC32_NIBBLE_TABLE[crc & 0x0F];
  }
  return ~crc;
}

#endif

}  // namespace esphome::crc32

#endif
