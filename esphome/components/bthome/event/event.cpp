#include "event.h"
#include "esphome/core/log.h"

namespace esphome::bthome::client {

ESPHOME_LOG_TAG(TAG, "bthome.event");

// Button codes from the BTHome v2 table. Shelly BLU firmware before 1.0.20 sent 0xFE for hold.
static constexpr uint8_t BUTTON_LONG_TRIPLE_PRESS = 0x06;
static constexpr uint8_t BUTTON_HOLD = 0x80;
static constexpr uint8_t BUTTON_HOLD_ALIAS = 0xFE;
static constexpr uint8_t DIMMER_ROTATE_RIGHT = 0x02;
// Position of hold_press in the event types of event/__init__.py.
static constexpr uint8_t HOLD_TYPE_INDEX = BUTTON_LONG_TRIPLE_PRESS;

bool BTHomeEvent::process_object(const BTHomeObject &object) {
  if (object.type == this->object_type_ && object.index == this->index_ && object.length > 0) {
    const uint8_t code = object.data[0];
    // Code 0 is "no event": a device with several buttons sends it for the buttons not pressed.
    if (this->object_type_ == BTHomeObjectType::BUTTON_EVENT) {
      if (code >= 1 && code <= BUTTON_LONG_TRIPLE_PRESS) {
        this->trigger(this->get_event_type(code - 1));
      } else if (code == BUTTON_HOLD || code == BUTTON_HOLD_ALIAS) {
        this->trigger(this->get_event_type(HOLD_TYPE_INDEX));
      }
    } else if (code >= 1 && code <= DIMMER_ROTATE_RIGHT) {
      this->trigger(this->get_event_type(code - 1));
    }
  }
  // Never claims the object: the hub hands each object to the first handler that does, and the events
  // for the other buttons of this type come after this one.
  return false;
}

void BTHomeEvent::log_config() {
  LOG_EVENT("    ", "Event", this);
  ESP_LOGCONFIG(TAG,
                "      Object type: 0x%02X\n"
                "      Index: %u",
                static_cast<unsigned>(this->object_type_), this->index_ + 1u);
}

}  // namespace esphome::bthome::client
