#include "reconfig_buttons.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char *const TAG = "LD2420.button";

namespace esphome {
namespace ld2420 {

void LD2420ApplyConfigButton::press_action() { this->parent_->apply_config_action(); }
void LD2420RevertConfigButton::press_action() { this->parent_->revert_config_action(); }
void LD2420RestartModuleButton::press_action() { this->parent_->restart_module_action(); }
void LD2420FactoryResetButton::press_action() { this->parent_->factory_reset_action(); }

}  // namespace ld2420
}  // namespace esphome
