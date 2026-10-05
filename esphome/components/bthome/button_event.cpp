#include "button_event.h"

namespace esphome::bthome {

bool PacketDedup::new_packet(uint8_t packet_id, uint32_t now) {
  const bool repeat =
      this->has_packet_id && packet_id == this->last_packet_id && (now - this->last_packet_ms) < PACKET_ID_RESET_MS;
  this->has_packet_id = true;
  this->last_packet_id = packet_id;
  this->last_packet_ms = now;
  return !repeat;
}

bool PacketDedup::new_gesture(uint32_t now) {
  if (this->has_gesture && (now - this->last_gesture_ms) < NO_PACKET_COOLDOWN_MS) {
    return false;
  }
  this->has_gesture = true;
  this->last_gesture_ms = now;
  return true;
}

}  // namespace esphome::bthome
