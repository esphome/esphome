#include <sstream>

#include "homeassistant_light.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/log.h"

namespace esphome {
namespace homeassistant {

static const char *const TAG = "homeassistant.light";

using namespace esphome::light;

optional<ColorMode> _parse_color_mode(const std::string color_mode) {
  if (color_mode == "unknown")
    return optional<ColorMode>(ColorMode::UNKNOWN);
  else if (color_mode == "onoff")
    return optional<ColorMode>(ColorMode::ON_OFF);
  else if (color_mode == "brightness")
    return optional<ColorMode>(ColorMode::BRIGHTNESS);
  else if (color_mode == "white")
    return optional<ColorMode>(ColorMode::WHITE);
  else if (color_mode == "color_temp")
    return optional<ColorMode>(ColorMode::COLOR_TEMPERATURE);
  else if (color_mode == "rgb" || color_mode == "hs" || color_mode == "xy")
    return optional<ColorMode>(ColorMode::RGB);
  else if (color_mode == "rgbw")
    return optional<ColorMode>(ColorMode::RGB_WHITE);
  else if (color_mode == "rgbww")
    return optional<ColorMode>(ColorMode::RGB_COLD_WARM_WHITE);

  return nullopt;
}

std::string _color_mode_name(const ColorMode color_mode) {
  switch (color_mode) {
    case ColorMode::UNKNOWN:
      return "unknown";
    case ColorMode::ON_OFF:
      return "onoff";
    case ColorMode::BRIGHTNESS:
      return "brightness";
    case ColorMode::WHITE:
      return "white";
    case ColorMode::COLOR_TEMPERATURE:
      return "color_temp";
    case ColorMode::RGB:
      return "rgb";
    case ColorMode::RGB_WHITE:
      return "rgbw";
    case ColorMode::RGB_COLD_WARM_WHITE:
      return "rgbww";
  }
  ESP_LOGW(TAG, "Unknown 'ColorMode' enum value '%d'!", color_mode);
  return "";
}

void HomeassistantLight::state_changed_(const std::string &state) {
  if (state == "None")
    return;
  switch (parse_on_off(state.c_str())) {
    case ParseOnOffState::PARSE_NONE:
      ESP_LOGW(TAG, "'%s': Can't parse '%s' as state!", this->entity_id_.c_str(), state.c_str());
      // this->invalidate_state();
      return;
    case ParseOnOffState::PARSE_ON:
      this->state_->turn_on().set_save(false).perform();
      break;
    case ParseOnOffState::PARSE_OFF:
      this->state_->turn_off().set_save(false).perform();
      break;
    case ParseOnOffState::PARSE_TOGGLE:
      this->state_->toggle().set_save(false).perform();
      break;
  }
  ESP_LOGD(TAG, "'%s': Got state %s", this->entity_id_.c_str(), state.c_str());
}

void HomeassistantLight::brightness_retrieved_(const std::string &brightness) {
  if (brightness == "None")
    return;
  auto brightness_value = parse_number<float>(brightness);
  if (!brightness_value.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert 'brightness' value '%s' to number!", this->entity_id_.c_str(),
             brightness.c_str());
    return;
  }
  ESP_LOGD(TAG, "'%s': Brightness retrieved: %s", this->entity_id_.c_str(), brightness.c_str());
  this->state_->make_call().set_brightness(brightness_value.value() / 255.f).set_save(false).perform();
}

void HomeassistantLight::color_temp_retrieved_(const std::string &color_temp) {
  if (color_temp == "None")
    return;
  auto color_temp_value = parse_number<float>(color_temp);
  if (!color_temp_value.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert 'color_temp' value '%s' to number!", this->entity_id_.c_str(),
             color_temp.c_str());
    return;
  }
  ESP_LOGD(TAG, "'%s': Color temperature retrieved: %s", this->entity_id_.c_str(), color_temp.c_str());
  this->state_->make_call().set_color_temperature(color_temp_value.value()).set_save(false).perform();
}

void HomeassistantLight::color_mode_retrieved_(const std::string &color_mode) {
  if (color_mode == "None")
    return;
  auto color_mode_value = _parse_color_mode(color_mode);
  if (!color_mode_value.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't parse 'color_mode' value '%s' as color mode!", this->entity_id_.c_str(),
             color_mode.c_str());
    return;
  }
  ESP_LOGD(TAG, "'%s': Color mode retrieved: %s", this->entity_id_.c_str(), color_mode.c_str());
  this->state_->make_call().set_color_mode(color_mode_value.value()).set_save(false).perform();
}

void HomeassistantLight::supported_color_modes_retrieved_(const std::string &supported_color_modes) {
  std::set<ColorMode> modes;
  std::stringstream color_modes(supported_color_modes);
  std::string color_mode_repr;
  while (std::getline(color_modes, color_mode_repr, ',')) {
    std::stringstream color_mode_repr_s(color_mode_repr);
    std::string color_mode;
    for (unsigned i = 0; i < 2; i++)  // <ColorMode.XY: 'xy'>
      std::getline(color_mode_repr_s, color_mode, '\'');
    auto color_mode_value = _parse_color_mode(color_mode);
    if (!color_mode_value.has_value()) {
      ESP_LOGW(TAG, "'%s': Can't parse 'supported_color_modes' value '%s' as color mode!", this->entity_id_.c_str(),
               color_mode.c_str());
      continue;
    }
    modes.insert(color_mode_value.value());
  }
  ESP_LOGD(TAG, "'%s': Supported color modes retrieved: %s", this->entity_id_.c_str(), supported_color_modes.c_str());
  this->traits_.set_supported_color_modes(modes);
}

void HomeassistantLight::min_mireds_retrieved_(const std::string &min_mireds) {
  auto min_mireds_value = parse_number<float>(min_mireds);
  if (!min_mireds_value.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert 'min_mireds' value '%s' to number!", this->entity_id_.c_str(),
             min_mireds.c_str());
    return;
  }
  ESP_LOGD(TAG, "'%s': Min mireds retrieved: %s", this->entity_id_.c_str(), min_mireds.c_str());
  this->traits_.set_min_mireds(min_mireds_value.value());
}

void HomeassistantLight::max_mireds_retrieved_(const std::string &max_mireds) {
  auto max_mireds_value = parse_number<float>(max_mireds);
  if (!max_mireds_value.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert 'max_mireds' value '%s' to number!", this->entity_id_.c_str(),
             max_mireds.c_str());
    return;
  }
  ESP_LOGD(TAG, "'%s': Max mireds retrieved: %s", this->entity_id_.c_str(), max_mireds.c_str());
  this->traits_.set_max_mireds(max_mireds_value.value());
}

void HomeassistantLight::setup() {
  api::global_api_server->get_home_assistant_state(
      this->entity_id_, optional<std::string>("supported_color_modes"),
      std::bind(&HomeassistantLight::supported_color_modes_retrieved_, this, std::placeholders::_1));
  api::global_api_server->get_home_assistant_state(
      this->entity_id_, optional<std::string>("min_mireds"),
      std::bind(&HomeassistantLight::min_mireds_retrieved_, this, std::placeholders::_1));
  api::global_api_server->get_home_assistant_state(
      this->entity_id_, optional<std::string>("max_mireds"),
      std::bind(&HomeassistantLight::max_mireds_retrieved_, this, std::placeholders::_1));

  api::global_api_server->get_home_assistant_state(
      this->entity_id_, optional<std::string>("color_mode"),
      std::bind(&HomeassistantLight::color_mode_retrieved_, this, std::placeholders::_1));
  api::global_api_server->get_home_assistant_state(
      this->entity_id_, optional<std::string>("brightness"),
      std::bind(&HomeassistantLight::brightness_retrieved_, this, std::placeholders::_1));
  api::global_api_server->get_home_assistant_state(
      this->entity_id_, optional<std::string>("color_temp"),
      std::bind(&HomeassistantLight::color_temp_retrieved_, this, std::placeholders::_1));

  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, nullopt, std::bind(&HomeassistantLight::state_changed_, this, std::placeholders::_1));
}

void HomeassistantLight::dump_config() {
  ESP_LOGCONFIG(TAG, "Homeassistant Light:");
  ESP_LOGCONFIG(TAG, "  Entity ID: '%s'", this->entity_id_.c_str());
}

float HomeassistantLight::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

void HomeassistantLight::setup_state(LightState *state) { this->state_ = state; }

void HomeassistantLight::write_state(LightState *state) {
  if (!api::global_api_server->is_connected()) {
    ESP_LOGE(TAG, "No clients connected to API server");
    return;
  }

  static constexpr auto SERVICE_ON = StringRef::from_lit("light.turn_on");
  static constexpr auto SERVICE_OFF = StringRef::from_lit("light.turn_off");
  static constexpr auto ENTITY_ID_KEY = StringRef::from_lit("entity_id");
  static constexpr auto COLOR_MODE_KEY = StringRef::from_lit("color_mode");
  static constexpr auto BRIGHTNESS_KEY = StringRef::from_lit("brightness");
  static constexpr auto COLOR_TEMP_KEY = StringRef::from_lit("color_temp");
  static constexpr auto COLOR_BRIGHTNESS_KEY = StringRef::from_lit("color_brightness");
  static constexpr auto RED_KEY = StringRef::from_lit("red");
  static constexpr auto GREEN_KEY = StringRef::from_lit("green");
  static constexpr auto BLUE_KEY = StringRef::from_lit("blue");
  static constexpr auto WHITE_KEY = StringRef::from_lit("white");
  static constexpr auto COLD_WHITE_KEY = StringRef::from_lit("cold_white");
  static constexpr auto WARM_WHITE_KEY = StringRef::from_lit("warm_white");

  api::HomeassistantServiceResponse resp;

  auto &entity_id = resp.data.emplace_back();
  entity_id.set_key(ENTITY_ID_KEY);
  entity_id.value = this->entity_id_;

  bool is_on;
  state->current_values_as_binary(&is_on);
  if (is_on) {
    resp.set_service(SERVICE_ON);

    auto color_mode = state->current_values.get_color_mode();

    if (color_mode != ColorMode::UNKNOWN) {
      auto &entity_color_mode = resp.data.emplace_back();
      entity_color_mode.set_key(COLOR_MODE_KEY);
      entity_color_mode.value = _color_mode_name(color_mode);
    }

    switch (color_mode) {
      case ColorMode::ON_OFF:
        break;

      case ColorMode::BRIGHTNESS:
      case ColorMode::WHITE:
        float brightness;
        state->current_values_as_brightness(&brightness);

        if (brightness) {
          auto &entity_brightness = resp.data.emplace_back();
          entity_brightness.set_key(color_mode == ColorMode::WHITE ? WHITE_KEY : BRIGHTNESS_KEY);
          entity_brightness.value = to_string((unsigned) (brightness * 255));
        }

        break;

      case ColorMode::RGB:
        float red, green, blue;
        state->current_values_as_rgb(&red, &green, &blue);
      case ColorMode::RGB_WHITE:
        float white;
        state->current_values_as_rgbw(&red, &green, &blue, &white);
      case ColorMode::RGB_COLD_WARM_WHITE:
        float warm_white;
        state->current_values_as_rgbww(&red, &green, &blue, &white, &warm_white);

        {
          auto &entity_red = resp.data.emplace_back();
          entity_red.set_key(RED_KEY);
          entity_red.value = to_string((unsigned) (red * 255));

          auto &entity_green = resp.data.emplace_back();
          entity_green.set_key(GREEN_KEY);
          entity_green.value = to_string((unsigned) (green * 255));

          auto &entity_blue = resp.data.emplace_back();
          entity_blue.set_key(BLUE_KEY);
          entity_blue.value = to_string((unsigned) (blue * 255));

          if (color_mode == ColorMode::RGB_COLD_WARM_WHITE) {
            auto &entity_warm_white = resp.data.emplace_back();
            entity_warm_white.set_key(WARM_WHITE_KEY);
            entity_warm_white.value = to_string((unsigned) (warm_white * 255));

            auto &entity_cold_white = resp.data.emplace_back();
            entity_cold_white.set_key(COLD_WHITE_KEY);
            entity_cold_white.value = to_string((unsigned) (white * 255));
          } else if (color_mode == ColorMode::RGB_WHITE) {
            auto &entity_white = resp.data.emplace_back();
            entity_white.set_key(WHITE_KEY);
            entity_white.value = to_string((unsigned) (white * 255));
          }
        }

        break;

      default:
        float color_temp, color_brightness;
        state->current_values_as_ct(&color_temp, &color_brightness);

        if (color_temp) {
          auto &entity_color_temp = resp.data.emplace_back();
          entity_color_temp.set_key(COLOR_TEMP_KEY);
          entity_color_temp.value =
              to_string((unsigned) (this->traits_.get_min_mireds() +
                                    color_temp * (this->traits_.get_max_mireds() - this->traits_.get_min_mireds())));
        }

        if (color_brightness) {
          auto &entity_color_brightness = resp.data.emplace_back();
          entity_color_brightness.set_key(COLOR_BRIGHTNESS_KEY);
          entity_color_brightness.value = to_string((unsigned) (color_brightness * 255));
        }
    }
  } else {
    resp.set_service(SERVICE_OFF);
  }

  api::global_api_server->send_homeassistant_service_call(resp);
}

}  // namespace homeassistant
}  // namespace esphome
