#pragma once

#include "codec.h"

#include <cstdint>

// Which button object to publish, and which advertisement is a repeat.
// No ESPHome headers, so the host test can compile it.

namespace esphome::bthome {

// BTHome v2 hold. Shelly BLU firmware before 1.0.20 sent 0xFE for the same gesture.
static constexpr uint8_t BUTTON_HOLD = 0x80;
static constexpr uint8_t BUTTON_HOLD_ALIAS = 0xFE;
static constexpr uint32_t NO_PACKET_COOLDOWN_MS = 1200;
// After this much silence a repeated packet id is new again, as in Home Assistant's parser.
static constexpr uint32_t PACKET_ID_RESET_MS = 4000;

// 1-based object order. Returns 0 when that button is absent.
// 0xFE is returned as 0x80. Every other value is left raw.
inline uint8_t button_event_at(const codec::Parsed &parsed, uint8_t index) {
  if (index < 1 || index > codec::MAX_BUTTONS || parsed.button_count < index) {
    return 0;
  }
  const uint8_t event = parsed.buttons[index - 1];
  if (event == BUTTON_HOLD_ALIAS) {
    return BUTTON_HOLD;
  }
  return event;
}

// One instance per binary sensor.
struct PacketDedup {
  // Every advertisement of the transmitter that has a packet id. False when the id equals
  // the previous one, unless the transmitter was silent for PACKET_ID_RESET_MS.
  bool new_packet(uint8_t packet_id, uint32_t now);
  // A matching gesture without a packet id. False inside the cooldown after the last one.
  bool new_gesture(uint32_t now);

  uint32_t last_packet_ms{0};
  uint32_t last_gesture_ms{0};
  uint8_t last_packet_id{0};
  bool has_packet_id{false};
  bool has_gesture{false};
};

}  // namespace esphome::bthome
