#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

namespace esphome::mk2pvrouter {
/*
 * Buffer sizes based on the mk2pvrouter telemetry protocol, as implemented by the
 * firmware's teleinfo.h (see github.com/FredM67/PVRouter-{1,3}-phase):
 * - Tags: max 4 chars (S_MC is longest), most are 1-2 chars (P, V1, R2, etc.)
 * - Values: max 6 digits signed (-10000), typical 1-5 digits. Energy (E) is a daily
 *   counter reset at midnight, so it stays well within 6 digits.
 * - Frame: STX + multiple lines (LF+tag+TAB+value+TAB+crc+CR) + ETX
 * - Line format: \n<tag>\t<value>\t<crc>\r (8-15 bytes per line)
 * - Multi-phase with all features: ~150-200 bytes
 */
static constexpr uint8_t MAX_TAG_SIZE = 8;     // S_MC (4) + digit (1) + null (1) + margin (2)
static constexpr uint8_t MAX_VAL_SIZE = 8;     // -10000 (6) + null (1) + margin (1)
static constexpr uint16_t MAX_BUF_SIZE = 256;  // Full frame with all features enabled

// Reads frames via UART, validates their CRC, and publishes tag/value pairs to the child entities.
class Mk2PVRouter : public Component, public uart::UARTDevice {
 public:
#ifdef USE_MK2PVROUTER_VALUE_PUBLISHER
  // Generated from the child entities; null when this hub has none
  HUB_PUBLISHER(value, const char *, const char *)
  HUB_PUBLISHER(dump, const char *)
#endif
  void loop() override;
  void dump_config() override;

 protected:
  static constexpr size_t CRC_SUFFIX_LEN = 1;

  enum class State : uint8_t {
    WAITING_FOR_START,
    START_FRAME_RECEIVED,
    END_FRAME_RECEIVED,
  };

  uint16_t buf_index_{0};
  State state_{State::WAITING_FOR_START};
  char tag_[MAX_TAG_SIZE];
  char val_[MAX_VAL_SIZE];
  uint8_t buf_[MAX_BUF_SIZE];  // Large buffer last to reduce padding

  bool read_chars_until_(bool drop, uint8_t c);
  uint8_t calculate_crc_(const uint8_t *grp, size_t grp_len);
  bool check_crc_(const uint8_t *grp, const uint8_t *grp_end);
  void process_group_(const uint8_t *grp, const uint8_t *grp_end);
  void publish_value_(const char *tag, const char *val);
};
}  // namespace esphome::mk2pvrouter
