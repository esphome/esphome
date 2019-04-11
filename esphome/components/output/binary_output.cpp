#include "binary_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace output {

static const char *TAG = "output.binary";

bool BinaryOutput::is_inverted() const { return this->inverted_; }
void BinaryOutput::set_inverted(bool inverted) { this->inverted_ = inverted; }
#ifdef USE_POWER_SUPPLY
void BinaryOutput::set_power_supply(PowerSupplyComponent *power_supply) { this->power_supply_ = power_supply; }
#endif
void BinaryOutput::turn_on() {
#ifdef USE_POWER_SUPPLY
  if (this->power_supply_ != nullptr && !this->has_requested_high_power_) {
    this->power_supply_->request_high_power();
    this->has_requested_high_power_ = true;
  }
#endif
  this->write_state(!this->inverted_);
}
void BinaryOutput::turn_off() {
#ifdef USE_POWER_SUPPLY
  if (this->power_supply_ != nullptr && this->has_requested_high_power_) {
    this->power_supply_->unrequest_high_power();
    this->has_requested_high_power_ = false;
  }
#endif
  this->write_state(this->inverted_);
}


}  // namespace output
}  // namespace esphome
