#include "dsp408_switch.h"
#include "esphome/core/log.h"

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

namespace esphome {
namespace dsp408 {

static const char *const TAG = "dsp408.switch";

static const char *kind_name(SwitchKind k) {
  switch (k) {
    case SwitchKind::MASTER_MUTE:
      return "master mute";
    case SwitchKind::CHANNEL_MUTE:
      return "channel mute";
    case SwitchKind::CHANNEL_POLAR:
      return "channel polar";
    case SwitchKind::INPUT_POLAR:
      return "input polar";
  }
  return "?";
}

void DSP408Switch::dump_config() {
  ESP_LOGCONFIG(TAG, "DSP-408 Switch (%s ch=%u): '%s'", kind_name(this->kind_), this->channel_,
                this->get_name().c_str());
}

void DSP408Switch::write_state(bool state) {
  if (this->parent_ == nullptr)
    return;
  this->publish_state(state);  // optimistic
  switch (this->kind_) {
    case SwitchKind::MASTER_MUTE:
      this->parent_->request_master_mute(state);
      break;
    case SwitchKind::CHANNEL_MUTE:
      this->parent_->request_channel_mute(this->channel_, state);
      break;
    case SwitchKind::CHANNEL_POLAR:
      this->parent_->request_channel_polar(this->channel_, state);
      break;
    case SwitchKind::INPUT_POLAR:
      this->parent_->request_input_polar(this->channel_, state);
      break;
  }
}

}  // namespace dsp408
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2/S3/P4
