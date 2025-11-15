#include "start_dynamic_background_correction_button.h"

#include "restart_button.h"

namespace esphome {
namespace ld2412 {

void StartDynamicBackgroundCorrectionButton::press_action() { this->parent_->start_dynamic_background_correction(); }

}  // namespace ld2412
}  // namespace esphome
