#include "crc32_test_component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::crc32_test_component {

static const char *const TAG = "crc32_test";

void CRC32TestComponent::setup() {
  ESP_LOGI(TAG, "CRC32 Helper Integration Test Starting");

  this->test_check_value();
  this->test_known_vectors();
  this->test_chaining();
  this->test_empty_input();

  ESP_LOGI(TAG, "CRC32 Integration Test Complete");
}

void CRC32TestComponent::test_check_value() {
  // The standard CRC-32/ISO-HDLC check value
  const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  bool passed = this->verify_crc32("Check value \"123456789\"", data, sizeof(data), 0xCBF43926);
  this->log_test_result("Check value", passed);
}

void CRC32TestComponent::test_known_vectors() {
  bool all_passed = true;

  const char *fox = "The quick brown fox jumps over the lazy dog";
  all_passed &= this->verify_crc32("Quick brown fox", reinterpret_cast<const uint8_t *>(fox), strlen(fox), 0x414FA339);

  uint8_t sequence[256];
  for (size_t i = 0; i < sizeof(sequence); i++)
    sequence[i] = i;
  all_passed &= this->verify_crc32("Bytes 0x00..0xFF", sequence, sizeof(sequence), 0x29058C73);

  const uint8_t zeroes[32] = {0};
  all_passed &= this->verify_crc32("32 zero bytes", zeroes, sizeof(zeroes), 0x190A55AD);

  this->log_test_result("Known vectors", all_passed);
}

void CRC32TestComponent::test_chaining() {
  // Feeding the data in pieces, seeding each call with the previous result,
  // must match a single call over the whole buffer
  const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  uint32_t crc = crc32(0, data, 4);
  crc = crc32(crc, data + 4, 3);
  crc = crc32(crc, data + 7, 2);

  bool passed = (crc == 0xCBF43926);
  if (!passed) {
    ESP_LOGE(TAG, "Chained CRC32: expected 0xCBF43926, got 0x%08" PRIX32, crc);
  }
  this->log_test_result("Chained updates", passed);
}

void CRC32TestComponent::test_empty_input() {
  bool all_passed = true;

  // A zero-length update returns the seed unchanged
  const uint8_t dummy[] = {0xAA};
  if (crc32(0, dummy, 0) != 0) {
    ESP_LOGE(TAG, "Empty input with seed 0 did not return 0");
    all_passed = false;
  }
  if (crc32(0x12345678, dummy, 0) != 0x12345678) {
    ESP_LOGE(TAG, "Empty input did not preserve the seed");
    all_passed = false;
  }

  this->log_test_result("Empty input", all_passed);
}

bool CRC32TestComponent::verify_crc32(const char *test_name, const uint8_t *data, size_t len, uint32_t expected,
                                      uint32_t crc) {
  uint32_t result = crc32(crc, data, len);
  bool passed = (result == expected);

  if (passed) {
    ESP_LOGI(TAG, "%s: PASS (0x%08" PRIX32 ")", test_name, result);
  } else {
    ESP_LOGE(TAG, "%s: FAIL - expected 0x%08" PRIX32 ", got 0x%08" PRIX32, test_name, expected, result);
  }

  return passed;
}

void CRC32TestComponent::log_test_result(const char *test_name, bool passed) {
  if (passed) {
    ESP_LOGI(TAG, "%s: ALL TESTS PASSED", test_name);
  } else {
    ESP_LOGE(TAG, "%s: SOME TESTS FAILED", test_name);
  }
}

}  // namespace esphome::crc32_test_component
