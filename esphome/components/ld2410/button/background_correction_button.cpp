#include "background_correction_button.h"

namespace esphome::ld2410 {

void BackgroundCorrectionButton::press_action() { this->parent_->start_background_correction(); }

}  // namespace esphome::ld2410
