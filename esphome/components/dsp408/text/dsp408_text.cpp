#include "dsp408_text.h"
#include "esphome/core/log.h"

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

namespace esphome {
namespace dsp408 {

static const char *const TAG = "dsp408.text";

void DSP408Text::dump_config() {
  if (this->kind_ == TextKind::PRESET_NAME) {
    ESP_LOGCONFIG(TAG, "DSP-408 Text (preset name): '%s'", this->get_name().c_str());
  } else {
    ESP_LOGCONFIG(TAG, "DSP-408 Text (channel %u name): '%s'", this->channel_, this->get_name().c_str());
  }
}

void DSP408Text::control(const std::string &value) {
  if (this->parent_ == nullptr)
    return;
  this->publish_state(value);  // optimistic
  if (this->kind_ == TextKind::PRESET_NAME) {
    this->parent_->request_preset_name(value);
  } else {
    this->parent_->request_channel_name(this->channel_, value);
  }
}

}  // namespace dsp408
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2/S3/P4
