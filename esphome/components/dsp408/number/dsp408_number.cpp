#include "dsp408_number.h"
#include "esphome/core/log.h"

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

namespace esphome {
namespace dsp408 {

static const char *const TAG = "dsp408.number";

static const char *kind_name(NumberKind k) {
  switch (k) {
    case NumberKind::MASTER_VOLUME:
      return "master volume";
    case NumberKind::CHANNEL_VOLUME:
      return "channel volume";
    case NumberKind::CHANNEL_DELAY:
      return "channel delay";
    case NumberKind::CHANNEL_HPF_FREQ:
      return "channel HPF freq";
    case NumberKind::CHANNEL_LPF_FREQ:
      return "channel LPF freq";
    case NumberKind::CHANNEL_COMP_ATTACK:
      return "comp attack";
    case NumberKind::CHANNEL_COMP_RELEASE:
      return "comp release";
    case NumberKind::CHANNEL_COMP_THRESHOLD:
      return "comp threshold";
  }
  return "?";
}

void DSP408Number::dump_config() {
  ESP_LOGCONFIG(TAG, "DSP-408 Number (%s ch=%u): '%s'", kind_name(this->kind_), this->channel_,
                this->get_name().c_str());
}

void DSP408Number::control(float value) {
  if (this->parent_ == nullptr)
    return;
  this->publish_state(value);  // optimistic; device-side ack will reaffirm
  switch (this->kind_) {
    case NumberKind::MASTER_VOLUME:
      this->parent_->request_master_volume(value);
      break;
    case NumberKind::CHANNEL_VOLUME:
      this->parent_->request_channel_volume(this->channel_, value);
      break;
    case NumberKind::CHANNEL_DELAY:
      this->parent_->request_channel_delay(this->channel_, static_cast<uint16_t>(value));
      break;
    case NumberKind::CHANNEL_HPF_FREQ:
      this->parent_->request_channel_hpf_freq(this->channel_, static_cast<uint16_t>(value));
      break;
    case NumberKind::CHANNEL_LPF_FREQ:
      this->parent_->request_channel_lpf_freq(this->channel_, static_cast<uint16_t>(value));
      break;
    case NumberKind::CHANNEL_COMP_ATTACK:
      this->parent_->request_compressor_attack(this->channel_, static_cast<uint16_t>(value));
      break;
    case NumberKind::CHANNEL_COMP_RELEASE:
      this->parent_->request_compressor_release(this->channel_, static_cast<uint16_t>(value));
      break;
    case NumberKind::CHANNEL_COMP_THRESHOLD:
      this->parent_->request_compressor_threshold(this->channel_, static_cast<uint8_t>(value));
      break;
  }
}

}  // namespace dsp408
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2/S3/P4
