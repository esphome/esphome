#include "button_event.h"

namespace esphome::bthome {

uint8_t button_event_at(const codec::Parsed &parsed, uint8_t index) {
  if (index < 1 || index > codec::MAX_BUTTONS || parsed.button_count < index) {
    return 0;
  }
  const uint8_t event = parsed.buttons[index - 1];
  if (event == BUTTON_HOLD_ALIAS) {
    return BUTTON_HOLD;
  }
  return event;
}

bool PacketDedup::accept(bool has_packet_id, uint8_t packet_id, uint32_t now) {
  if (has_packet_id) {
    if (this->has_packet_id && packet_id == this->last_packet_id) {
      return false;
    }
    this->has_packet_id = true;
    this->last_packet_id = packet_id;
    this->has_emit = true;
    this->last_emit_ms = now;
    return true;
  }
  // First advertisement has nothing to compare against. A later one inside
  // the window is the same gesture sent again.
  if (this->has_emit && (now - this->last_emit_ms) < NO_PACKET_COOLDOWN_MS) {
    return false;
  }
  this->has_emit = true;
  this->last_emit_ms = now;
  return true;
}

}  // namespace esphome::bthome
