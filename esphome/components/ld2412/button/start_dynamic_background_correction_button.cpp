#include "start_dynamic_background_correction_button.h"

#include "restart_button.h"

namespace esphome::ld2412 {

void StartDynamicBackgroundCorrectionButton::press_action() { this->parent_->start_dynamic_background_correction(); }

}  // namespace esphome::ld2412
