#include "esphome/core/log.h"
#include "tuya_light.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.light";

bool TuyaLight::shouldIgnoreDimmerCommand()
{
  if (this->ignore_dimmer_cmd_timeout > millis())
  {
      ESP_LOGV(TAG, "dimmer_cmd: ignored");
      return true;
  }

  this->ignore_write_state = true; // Ignore next write_state call
  return false;
}

void TuyaLight::setup() {
  if (this->dimmer_id_.has_value()) {
    this->parent_->register_listener(*this->dimmer_id_, [this](TuyaDatapoint datapoint) {
      if (this->shouldIgnoreDimmerCommand())
        return;

      int brightness_int = map(datapoint.value_uint, this->min_value_, this->max_value_, 0, 255);
      brightness_int = max(brightness_int, 0);
      auto brightness = float(brightness_int) / 255.0f;

      auto call = this->state_->make_call();
      call.set_brightness(brightness_float);
      call.perform();
    });
  }
  if (switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](TuyaDatapoint datapoint) {
      if (this->shouldIgnoreDimmerCommand())
        return;

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
  if (this->ignore_write_state)
  {
      this->ignore_write_state = false;
      ESP_LOGV(TAG, "write_state: ignored");
      return;
  }

  float brightness;
  state->current_values_as_brightness(&brightness);

  this->ignore_dimmer_cmd_timeout = millis() + 250; // Ignore serial received dim commands for the next 250ms

  if (brightness == 0.0f) {
    // turning off, first try via switch (if exists), then dimmer
    if (switch_id_.has_value()) {
      TuyaDatapoint datapoint{};
      datapoint.id = *this->switch_id_;
      datapoint.type = TuyaDatapointType::BOOLEAN;
      datapoint.value_bool = false;

      parent_->set_datapoint_value(datapoint);
    } else if (dimmer_id_.has_value()) {
      TuyaDatapoint datapoint{};
      datapoint.id = *this->dimmer_id_;
      datapoint.type = TuyaDatapointType::INTEGER;
      datapoint.value_int = 0;
      parent_->set_datapoint_value(datapoint);
    }
    return;
  }

  auto brightness_int = static_cast<uint32_t>(brightness * 255);
  brightness_int = map(brightness_int, 0, 255, this->min_value_, this->max_value_);

  if (this->dimmer_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->dimmer_id_;
    datapoint.type = TuyaDatapointType::INTEGER;
    datapoint.value_int = brightness_int;
    parent_->set_datapoint_value(datapoint);
  }
  if (this->switch_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->switch_id_;
    datapoint.type = TuyaDatapointType::BOOLEAN;
    datapoint.value_bool = true;
    parent_->set_datapoint_value(datapoint);
  }
}

}  // namespace tuya
}  // namespace esphome
