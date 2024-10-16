#include "audio_deviation_number.h"

namespace esphome {
namespace si4713 {

void AudioDeviationNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_audio_deviation(value);
}

}  // namespace si4713
}  // namespace esphome