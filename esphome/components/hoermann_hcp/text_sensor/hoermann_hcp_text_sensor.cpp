#include "hoermann_hcp_text_sensor.h"

#include <cstring>

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::hoermann_hcp {

static const char *const TAG = "hoermann_hcp.text_sensor";

// Indexed by DoorState. Each fits the 15 characters std::string keeps inline, so publishing never allocates.
PROGMEM_STRING_TABLE(DoorStateStrings, "Open", "Opening", "Closed", "Closing", "Half open", "Moving to vent",
                     "Vent position", "Moving to half", "Stopped");
static_assert(DoorStateStrings::COUNT == static_cast<size_t>(DoorState::STOPPED) + 1,
              "DoorStateStrings must list every DoorState in order");

void HoermannHcpDoorStateTextSensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_state_(); });
  // A state decoded before the callback was added would otherwise wait for the next change.
  this->update_from_state_();
}

void HoermannHcpDoorStateTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Hoermann HCP Door State", this); }

void HoermannHcpDoorStateTextSensor::update_from_state_() {
  // The last state stays while the bus controller is gone, and is published again once it is back.
  if (!this->parent_->is_valid()) {
    this->published_ = false;
    return;
  }
  if (!this->parent_->is_door_state_known())
    return;
  // Any hub change runs this, so only a new door state is published.
  const DoorState state = this->parent_->get_door_state();
  if (this->published_ && state == this->published_state_)
    return;
  this->published_ = true;
  this->published_state_ = state;
  char text[16];
  ESPHOME_strncpy_P(text,
                    reinterpret_cast<ESPHOME_PGM_P>(
                        DoorStateStrings::get_progmem_str(static_cast<uint8_t>(state), DoorStateStrings::LAST_INDEX)),
                    sizeof(text));
  text[sizeof(text) - 1] = '\0';
  this->publish_state(text);
}

}  // namespace esphome::hoermann_hcp
