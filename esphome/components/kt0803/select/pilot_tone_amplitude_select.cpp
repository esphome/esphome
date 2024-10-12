#include "pilot_tone_amplitude_select.h"

namespace esphome {
namespace kt0803 {

void PilotToneAmplitudeSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_pilot_tone_amplitude((PilotToneAmplitude) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
