#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"

#include <array>

namespace esphome::toptronic {

class TopTronic : public Component {
 public:
  TopTronic(canbus::Canbus *canbus, uint16_t device_id);

  void dump_config() override;

 protected:
  // The 5-bit frame count field caps a message at 31 frames of up to 7 payload bytes
  static constexpr size_t MAX_FRAMES_PER_MESSAGE = 31;
  static constexpr size_t MAX_CONT_FRAME_PAYLOAD = 7;
  static constexpr size_t MAX_MESSAGE_LEN = MAX_FRAMES_PER_MESSAGE * MAX_CONT_FRAME_PAYLOAD;
  static_assert(MAX_MESSAGE_LEN <= UINT8_MAX, "Slot::len is a uint8_t");
  static constexpr uint32_t MAX_PENDING_AGE_MS = 5000;
  // One sender answers requests one after another; a second slot covers an abandoned message
  static constexpr size_t NUM_SLOTS = 2;

  struct Slot {
    uint32_t last_update_ms;
    uint8_t len;
    uint8_t tag;
    uint8_t remaining;  // continuation frames still expected, 0 when free
    uint8_t data[MAX_MESSAGE_LEN];

    bool is_live(uint32_t now) const {
      return this->remaining != 0 && now - this->last_update_ms <= MAX_PENDING_AGE_MS;
    }
  };

  /// Feeds one frame from this device into reassembly. Returns true and points msg/len at the payload when
  /// a message is complete (CRC already checked and stripped); the pointer is valid until the next frame.
  bool parse_frame_(const uint8_t *data, size_t size, uint32_t can_id, const uint8_t **msg, size_t *len);
  void interpret_message_(const uint8_t *data, size_t len);

  std::array<Slot, NUM_SLOTS> slots_{};
  uint16_t device_id_;
};

}  // namespace esphome::toptronic
