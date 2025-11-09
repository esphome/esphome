#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace crc8_test_component {

class CRC8TestComponent : public Component {
 public:
  void setup() override;

 private:
  void test_crc8_dallas_maxim();
  void test_crc8_sensirion_style();
  void test_crc8_pec_style();
  void test_crc8_parameter_equivalence();
  void test_crc8_edge_cases();
  void test_component_compatibility();
  void test_old_vs_new_implementations();

  void log_test_result(const char *test_name, bool passed);
  bool verify_crc8(const char *test_name, const uint8_t *data, uint8_t len, uint8_t expected, uint8_t crc = 0x00,
                   uint8_t poly = 0x8C, bool msb_first = false);
};

}  // namespace crc8_test_component
}  // namespace esphome
