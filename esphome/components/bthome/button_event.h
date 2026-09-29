#pragma once

#include "codec.h"

#include <cstdint>

// Which button object to publish, and which advertisement is a repeat.
// No ESPHome headers, so the host test can compile it.

namespace esphome::bthome {

// BTHome v2 hold. Older transmitters sent 0xFE for the same gesture.
static constexpr uint8_t BUTTON_HOLD = 0x80;
static constexpr uint8_t BUTTON_HOLD_ALIAS = 0xFE;
static constexpr uint32_t NO_PACKET_COOLDOWN_MS = 1200;

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

// One instance per binary sensor. Packet id wins. Without one, a cooldown
// drops the copies a transmitter sends for the same gesture.
struct PacketDedup {
  bool accept(bool has_packet_id, uint8_t packet_id, uint32_t now);

  uint32_t last_emit_ms{0};
  uint8_t last_packet_id{0};
  bool has_packet_id{false};
  bool has_emit{false};
};

}  // namespace esphome::bthome
