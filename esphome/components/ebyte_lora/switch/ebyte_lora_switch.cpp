#include "ebyte_lora_switch.h"

namespace esphome {
namespace ebyte_lora {
static const char *const TAG_SWITCH = "ebyte_lora_switch";
void EbyteLoraSwitch::write_state(bool state) { this->parent_->digital_write(this->pin_, state); }
void EbyteLoraSwitch::dump_config() { LOG_SWITCH(TAG_SWITCH, "Ebyte Lora Switch", this); }
}  // namespace ebyte_lora
}  // namespace esphome
