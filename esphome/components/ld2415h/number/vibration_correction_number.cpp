#include "vibration_correction_number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2415h {

static const char *const TAG = "LD2415H.vibration_correction_number";

void VibrationCorrectionNumber::control(float correction) {
  this->publish_state(correction);
  this->parent_->set_vibration_correction(correction);
}

}  // namespace ld2415h
}  // namespace esphome
