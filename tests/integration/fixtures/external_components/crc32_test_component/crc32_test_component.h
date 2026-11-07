#pragma once

#include "esphome/core/component.h"

namespace esphome::crc32_test_component {

class CRC32TestComponent : public Component {
 public:
  void setup() override;

 private:
  void test_check_value();
  void test_known_vectors();
  void test_chaining();
  void test_empty_input();

  void log_test_result(const char *test_name, bool passed);
  bool verify_crc32(const char *test_name, const uint8_t *data, size_t len, uint32_t expected, uint32_t crc = 0);
};

}  // namespace esphome::crc32_test_component
