#include "crc32.h"

#ifdef USE_TFLITE_MICRO_HELPER
#include "esphome/core/log.h"

namespace esphome::tflite_model_loader {

ESPHOME_LOG_TAG(TAG, "tflite_model_loader");

uint32_t calculate_crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc >>= 1;
      }
    }
  }
  return ~crc;
}

bool verify_model_crc(const uint8_t *model_data, size_t length, uint32_t expected_crc) {
  uint32_t crc = calculate_crc32(model_data, length);
  ESP_LOGI(TAG, "Computed CRC32: 0x%08X", crc);
  if (expected_crc == 0) {
    ESP_LOGW(TAG, "No expected CRC32 provided -- skipping verification");
    return true;
  }
  if (crc != expected_crc) {
    ESP_LOGE(TAG, "Model CRC32 mismatch! Expected: 0x%08X, Got: 0x%08X", expected_crc, crc);
    return false;
  }
  ESP_LOGI(TAG, "Model CRC32 verification passed");
  return true;
}

}  // namespace esphome::tflite_model_loader

#endif  // USE_TFLITE_MICRO_HELPER
