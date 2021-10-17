#include "esphome/core/log.h"
#include "tuya_light.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.light";

void TuyaLight::setup() {
  if (this->color_temperature_id_.has_value()) {
    this->parent_->register_listener(*this->color_temperature_id_, [this](const TuyaDatapoint &datapoint) {
      auto datapoint_value = datapoint.value_uint;
      if (this->color_temperature_invert_) {
        datapoint_value = this->color_temperature_max_value_ - datapoint_value;
      }
      auto call = this->state_->make_call();
      call.set_color_temperature(this->cold_white_temperature_ +
                                 (this->warm_white_temperature_ - this->cold_white_temperature_) *
                                     (float(datapoint_value) / this->color_temperature_max_value_));
      call.perform();
    });
  }
  if (this->dimmer_id_.has_value()) {
    this->parent_->register_listener(*this->dimmer_id_, [this](const TuyaDatapoint &datapoint) {
      auto call = this->state_->make_call();
      call.set_brightness(float(datapoint.value_uint) / this->max_value_);
      call.perform();
    });
  }
  if (switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](const TuyaDatapoint &datapoint) {
      auto call = this->state_->make_call();
      call.set_state(datapoint.value_bool);
      call.perform();
    });
  }
  if (rgb_id_.has_value()) {
    this->parent_->register_listener(*this->rgb_id_, [this](const TuyaDatapoint &datapoint) {
      auto red = parse_hex(datapoint.value_string, 0, 2);
      auto green = parse_hex(datapoint.value_string, 2, 2);
      auto blue = parse_hex(datapoint.value_string, 4, 2);
      if (red.has_value() && green.has_value() && blue.has_value()) {
        auto call = this->state_->make_call();
        call.set_rgb(float(*red) / 255, float(*green) / 255, float(*blue) / 255);
        call.perform();
      }
    });
  } else if (hsv_id_.has_value()) {
    this->parent_->register_listener(*this->hsv_id_, [this](const TuyaDatapoint &datapoint) {
      auto hue = parse_hex(datapoint.value_string, 0, 4);
      auto saturation = parse_hex(datapoint.value_string, 4, 4);
      auto value = parse_hex(datapoint.value_string, 8, 4);
      if (hue.has_value() && saturation.has_value() && value.has_value()) {
        float red, green, blue;
        hsv_to_rgb(*hue, float(*saturation) / 1000, float(*value) / 1000, red, green, blue);
        auto call = this->state_->make_call();
        call.set_rgb(red, green, blue);
        call.perform();
      }
    });
  }
  if (min_value_datapoint_id_.has_value()) {
    parent_->set_integer_datapoint_value(*this->min_value_datapoint_id_, this->min_value_);
  }
}

void TuyaLight::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Dimmer:");
  if (this->dimmer_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Dimmer has datapoint ID %u", *this->dimmer_id_);
  if (this->switch_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Switch has datapoint ID %u", *this->switch_id_);
  if (this->rgb_id_.has_value())
    ESP_LOGCONFIG(TAG, "   RGB has datapoint ID %u", *this->rgb_id_);
  else if (this->hsv_id_.has_value())
    ESP_LOGCONFIG(TAG, "   HSV has datapoint ID %u", *this->hsv_id_);
}

light::LightTraits TuyaLight::get_traits() {
  auto traits = light::LightTraits();
  if (this->color_temperature_id_.has_value() && this->dimmer_id_.has_value()) {
    if (this->rgb_id_.has_value() || this->hsv_id_.has_value()) {
      if (this->color_interlock_)
        traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::COLOR_TEMPERATURE});
      else
        traits.set_supported_color_modes(
            {light::ColorMode::RGB_COLOR_TEMPERATURE, light::ColorMode::COLOR_TEMPERATURE});
    } else
      traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
    traits.set_min_mireds(this->cold_white_temperature_);
    traits.set_max_mireds(this->warm_white_temperature_);
  } else if (this->rgb_id_.has_value() || this->hsv_id_.has_value()) {
    if (this->dimmer_id_.has_value()) {
      if (this->color_interlock_)
        traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::WHITE});
      else
        traits.set_supported_color_modes({light::ColorMode::RGB_WHITE});
    } else
      traits.set_supported_color_modes({light::ColorMode::RGB});
  } else if (this->dimmer_id_.has_value()) {
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  } else {
    traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  }
  return traits;
}

void TuyaLight::setup_state(light::LightState *state) { state_ = state; }

void TuyaLight::write_state(light::LightState *state) {
  float red = 0.0f, green = 0.0f, blue = 0.0f;
  float color_temperature = 0.0f, brightness = 0.0f;

  if (this->rgb_id_.has_value() || this->hsv_id_.has_value()) {
    if (this->color_temperature_id_.has_value()) {
      state->current_values_as_rgbct(&red, &green, &blue, &color_temperature, &brightness);
    } else if (this->dimmer_id_.has_value()) {
      state->current_values_as_rgbw(&red, &green, &blue, &brightness);
    } else {
      state->current_values_as_rgb(&red, &green, &blue);
    }
  } else if (this->color_temperature_id_.has_value()) {
    state->current_values_as_ct(&color_temperature, &brightness);
  } else {
    state->current_values_as_brightness(&brightness);
  }

  if (!state->current_values.is_on() && this->switch_id_.has_value()) {
    parent_->set_boolean_datapoint_value(*this->switch_id_, false);
    return;
  }

  if (brightness > 0.0f || !color_interlock_) {
    if (this->color_temperature_id_.has_value()) {
      uint32_t color_temp_int = static_cast<uint32_t>(color_temperature * this->color_temperature_max_value_);
      if (this->color_temperature_invert_) {
        color_temp_int = this->color_temperature_max_value_ - color_temp_int;
      }
      parent_->set_integer_datapoint_value(*this->color_temperature_id_, color_temp_int);
    }

    if (this->dimmer_id_.has_value()) {
      auto brightness_int = static_cast<uint32_t>(brightness * this->max_value_);
      brightness_int = std::max(brightness_int, this->min_value_);

      parent_->set_integer_datapoint_value(*this->dimmer_id_, brightness_int);
    }
  }

  if (brightness == 0.0f || !color_interlock_) {
    if (this->rgb_id_.has_value()) {
      char buffer[7];
      sprintf(buffer, "%02X%02X%02X", int(red * 255), int(green * 255), int(blue * 255));
      std::string rgb_value = buffer;
      this->parent_->set_string_datapoint_value(*this->rgb_id_, rgb_value);
    } else if (this->hsv_id_.has_value()) {
      int hue;
      float saturation, value;
      rgb_to_hsv(red, green, blue, hue, saturation, value);
      char buffer[13];
      sprintf(buffer, "%04X%04X%04X", hue, int(saturation * 1000), int(value * 1000));
      std::string hsv_value = buffer;
      this->parent_->set_string_datapoint_value(*this->hsv_id_, hsv_value);
    }
  }

  if (this->switch_id_.has_value()) {
    parent_->set_boolean_datapoint_value(*this->switch_id_, true);
  }
}

}  // namespace tuya
}  // namespace esphome
