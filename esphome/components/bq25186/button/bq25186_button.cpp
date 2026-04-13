#include "bq25186_button.h"

namespace esphome::bq25186 {

void BQ25186SoftwareResetButton::press_action() { this->parent_->trigger_software_reset(); }

void BQ25186ShutdownButton::press_action() { this->parent_->trigger_shutdown_mode(); }

void BQ25186ShipModeButton::press_action() { this->parent_->trigger_ship_mode(); }

void BQ25186HardwareResetButton::press_action() { this->parent_->trigger_hardware_reset(); }

}  // namespace esphome::bq25186
