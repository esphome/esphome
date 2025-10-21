#include "reset_radar_button.h"

namespace esphome {
namespace seeed_mr60fda2 {

void ResetRadarButton::press_action() { this->parent_->factory_reset(); }

}  // namespace seeed_mr60fda2
}  // namespace esphome
