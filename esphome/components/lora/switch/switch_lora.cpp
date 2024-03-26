#include "switch_lora.h"

namespace esphome {
namespace lora {
static const char *const TAG_SWITCH = "lora.switch";

void LoraSwitch::write_state(bool state) {
  this->parent_->digital_write(this->pin_, state != this->inverted_);
  // finally set it
  this->publish_state(state);
}
void LoraSwitch::dump_config() { LOG_SWITCH(TAG_SWITCH, "Lora Switch", this); }
}  // namespace lora
}  // namespace esphome
