#include "crc32.h"

#ifdef USE_TFLITE_MICRO_HELPER
#include <cinttypes>
#include "esphome/core/log.h"
#ifdef USE_ESP32
#include "esp_rom_crc.h"
#endif

namespace esphome::tflite_model_loader {

ESPHOME_LOG_TAG(TAG, "tflite_model_loader");

uint32_t calculate_crc32(const uint8_t *data, size_t length) {
  if (data == nullptr || length == 0)
    return 0;
#ifdef USE_ESP32
  // Same reflected CRC32 as the loop below, computed by the ROM routine.
  return esp_rom_crc32_le(0, data, static_cast<uint32_t>(length));
#else
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
  }
  return ~crc;
#endif
}

bool verify_model_crc(const uint8_t *model_data, size_t length, uint32_t expected_crc) {
  if (expected_crc == 0) {
    ESP_LOGD(TAG, "No expected CRC32 provided, skipping verification");
    return true;
  }
  if (model_data == nullptr) {
    ESP_LOGE(TAG, "Model data pointer is NULL, cannot verify CRC32");
    return false;
  }

  const uint32_t crc = calculate_crc32(model_data, length);
  if (crc != expected_crc) {
    ESP_LOGE(TAG, "Model CRC32 mismatch: expected 0x%08" PRIX32 ", got 0x%08" PRIX32, expected_crc, crc);
    return false;
  }
  ESP_LOGD(TAG, "Model CRC32 verified: 0x%08" PRIX32, crc);
  return true;
}

}  // namespace esphome::tflite_model_loader

#endif  // USE_TFLITE_MICRO_HELPER
