#include "button.h"

namespace esphome {
namespace dfrobot_c4001 {
void ConfigSaveButton::press_action() { this->parent_->config_save(); }
void FactoryResetButton::press_action() { this->parent_->factory_reset(); }
void RestartButton::press_action() { this->parent_->restart(); }
}  // namespace dfrobot_c4001
}  // namespace esphome
