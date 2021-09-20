#include "esphome/core/log.h"
#include "tuya_light.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.light";

uint8_t hexchar_to_int(const std::string &dp_data_string, const uint8_t index_in_string) {
  uint8_t out = dp_data_string[index_in_string];
  if (out >= '0' && out <= '9')
    return (out - '0');
  if (out >= 'A' && out <= 'F')
    return (10 + (out - 'A'));
  if (out >= 'a' && out <= 'f')
    return (10 + (out - 'a'));
  return out;
}

uint8_t hexpair_to_int(const std::string &dp_data_string, const uint8_t index_in_string) {
  uint8_t a = hexchar_to_int(dp_data_string, index_in_string);
  uint8_t b = hexchar_to_int(dp_data_string, index_in_string + 1);
  return (a << 4) | b;
}

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
      uint8_t red = hexpair_to_int(datapoint.value_string, 0);
      uint8_t green = hexpair_to_int(datapoint.value_string, 2);
      uint8_t blue = hexpair_to_int(datapoint.value_string, 4);
      auto call = this->state_->make_call();
      call.set_rgb(float(red) / 255, float(green) / 255, float(blue) / 255);
      call.perform();
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
}

light::LightTraits TuyaLight::get_traits() {
  auto traits = light::LightTraits();
  if (this->color_temperature_id_.has_value() && this->dimmer_id_.has_value()) {
    if (this->rgb_id_.has_value()) {
      if (this->color_interlock_)
        traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::COLOR_TEMPERATURE});
      else
        traits.set_supported_color_modes(
            {light::ColorMode::RGB_COLOR_TEMPERATURE, light::ColorMode::COLOR_TEMPERATURE});
    } else
      traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
    traits.set_min_mireds(this->cold_white_temperature_);
    traits.set_max_mireds(this->warm_white_temperature_);
  } else if (this->rgb_id_.has_value()) {
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

  if (this->rgb_id_.has_value()) {
    if (this->color_temperature_id_.has_value()) {
      state->current_values_as_rgbct(&red, &green, &blue, &color_temperature, &brightness);
    } else if (this->dimmer_id_.has_value()) {
      state->current_values_as_rgbw(&red, &green, &blue, &brightness, this->color_interlock_);
    } else {
      state->current_values_as_rgb(&red, &green, &blue);
    }
  } else if (this->color_temperature_id_.has_value()) {
    state->current_values_as_ct(&color_temperature, &brightness);
  } else {
    state->current_values_as_brightness(&brightness);
  }

  if (!state->current_values.is_on()) {
    // turning off, first try via switch (if exists), then dimmer
    if (switch_id_.has_value()) {
      parent_->set_boolean_datapoint_value(*this->switch_id_, false);
      return;
    }
    if (dimmer_id_.has_value()) {
      parent_->set_integer_datapoint_value(*this->dimmer_id_, 0);
    }
    if (rgb_id_.has_value()) {
      parent_->set_string_datapoint_value(*this->rgb_id_, "000000");
    }
    return;
  }

  if (brightness > 0.0f) {
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

  if (this->rgb_id_.has_value() && (brightness == 0.0f || !this->color_interlock_)) {
    char buffer[7];
    sprintf(buffer, "%02X%02X%02X", int(red * 255), int(green * 255), int(blue * 255));
    std::string value = buffer;
    this->parent_->set_string_datapoint_value(*this->rgb_id_, value);
  }

  if (this->switch_id_.has_value()) {
    parent_->set_boolean_datapoint_value(*this->switch_id_, true);
  }
}

}  // namespace tuya
}  // namespace esphome
