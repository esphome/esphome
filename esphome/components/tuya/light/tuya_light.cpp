#include "esphome/core/log.h"
#include "tuya_light.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.light";

void TuyaLight::setup() {
  if (this->dimmer_id_.has_value()) {
    this->parent_->register_listener(*this->dimmer_id_, [this](TuyaDatapoint datapoint) {
      this->inhibit_next_send_ = true;

      // Clip value to expected range, allowing for inverted range
      auto lower = std::min(this->min_value_, this->max_value_);
      auto upper = std::max(this->min_value_, this->max_value_);
      auto value = std::min(upper, std::max(lower, static_cast<int32_t>(datapoint.value_uint)));

      auto call = this->state_->make_call();
      call.set_brightness(float(value - this->min_value_) / (this->max_value_ - this->min_value_)); // Don't use lower/upper here to allow inversion
      call.perform();
    });
  }
  if (switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](TuyaDatapoint datapoint) {
      this->inhibit_next_send_ = true;

      auto call = this->state_->make_call();
      call.set_state(datapoint.value_bool);
      call.perform();
    });
  }
}

void TuyaLight::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Dimmer:");
  if (this->dimmer_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Dimmer has datapoint ID %u", *this->dimmer_id_);
  if (this->switch_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Switch has datapoint ID %u", *this->switch_id_);
}

light::LightTraits TuyaLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supports_brightness(this->dimmer_id_.has_value());
  return traits;
}

void TuyaLight::setup_state(light::LightState *state) { state_ = state; }

void TuyaLight::write_state(light::LightState *state) {
  // Don't echo state back to MCU
  if (this->inhibit_next_send_) {
    this->inhibit_next_send_ = false;
    return;
  }

  float brightness;
  state->current_values_as_brightness(&brightness);
  auto brightness_int = static_cast<uint32_t>(brightness * (this->max_value_ - this->min_value_) + this->min_value_);
  bool is_off = brightness == 0.0f;

  if (this->dimmer_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->dimmer_id_;
    datapoint.type = TuyaDatapointType::INTEGER;
    datapoint.value_int = is_off ? 0 : brightness_int; // Ensure turn off even when min value is set
    parent_->set_datapoint_value(datapoint);
  }

  if (this->switch_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->switch_id_;
    datapoint.type = TuyaDatapointType::BOOLEAN;
    datapoint.value_bool = is_off != 0.0f;
    parent_->set_datapoint_value(datapoint);
  }
}

}  // namespace tuya
}  // namespace esphome
