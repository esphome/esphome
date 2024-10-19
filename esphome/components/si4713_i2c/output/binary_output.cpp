#include "binary_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace si4713 {

static const char *const TAG = "si4713";

void BinaryOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Si4713 Output:");
  LOG_BINARY_OUTPUT(this);
  ESP_LOGCONFIG(TAG, "  Pin: %d", this->pin_);
}

void BinaryOutput::write_state(bool state) {
  this->parent_->set_output_gpio(this->pin_, state);
}

}  // namespace si4713
}  // namespace esphome





