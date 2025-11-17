#include "light_json_schema.h"
#include "light_output.h"

#ifdef USE_JSON

namespace esphome {
namespace light {

// See https://www.home-assistant.io/integrations/light.mqtt/#json-schema for documentation on the schema

// Lookup table for color mode strings
static constexpr const char *get_color_mode_json_str(ColorMode mode) {
  switch (mode) {
    case ColorMode::ON_OFF:
      return "onoff";
    case ColorMode::BRIGHTNESS:
      return "brightness";
    case ColorMode::WHITE:
      return "white";  // not supported by HA in MQTT
    case ColorMode::COLOR_TEMPERATURE:
      return "color_temp";
    case ColorMode::COLD_WARM_WHITE:
      return "cwww";  // not supported by HA
    case ColorMode::RGB:
      return "rgb";
    case ColorMode::RGB_WHITE:
      return "rgbw";
    case ColorMode::RGB_COLOR_TEMPERATURE:
      return "rgbct";  // not supported by HA
    case ColorMode::RGB_COLD_WARM_WHITE:
      return "rgbww";
    default:
      return nullptr;
  }
}

void LightJSONSchema::dump_json(LightState &state, JsonObject root) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  if (state.supports_effects()) {
    root["effect"] = state.get_effect_name();
    root["effect_index"] = state.get_current_effect_index();
    root["effect_count"] = state.get_effect_count();
  }

  auto values = state.remote_values;

  const auto color_mode = values.get_color_mode();
  const char *mode_str = get_color_mode_json_str(color_mode);
  if (mode_str != nullptr) {
    root["color_mode"] = mode_str;
  }

  if (color_mode & ColorCapability::ON_OFF)
    root["state"] = (values.get_state() != 0.0f) ? "ON" : "OFF";
  if (color_mode & ColorCapability::BRIGHTNESS)
    root["brightness"] = to_uint8_scale(values.get_brightness());

  JsonObject color = root["color"].to<JsonObject>();
  if (color_mode & ColorCapability::RGB) {
    float color_brightness = values.get_color_brightness();
    color["r"] = to_uint8_scale(color_brightness * values.get_red());
    color["g"] = to_uint8_scale(color_brightness * values.get_green());
    color["b"] = to_uint8_scale(color_brightness * values.get_blue());
  }
  if (color_mode & ColorCapability::WHITE) {
    uint8_t white_val = to_uint8_scale(values.get_white());
    color["w"] = white_val;
    root["white_value"] = white_val;  // legacy API
  }
  if (color_mode & ColorCapability::COLOR_TEMPERATURE) {
    // this one isn't under the color subkey for some reason
    root["color_temp"] = uint32_t(values.get_color_temperature());
  }
  if (color_mode & ColorCapability::COLD_WARM_WHITE) {
    color["c"] = to_uint8_scale(values.get_cold_white());
    color["w"] = to_uint8_scale(values.get_warm_white());
  }
}

void LightJSONSchema::parse_color_json(LightState &state, LightCall &call, JsonObject root) {
  if (root["state"].is<const char *>()) {
    auto val = parse_on_off(root["state"]);
    switch (val) {
      case PARSE_ON:
        call.set_state(true);
        break;
      case PARSE_OFF:
        call.set_state(false);
        break;
      case PARSE_TOGGLE:
        call.set_state(!state.remote_values.is_on());
        break;
      case PARSE_NONE:
        break;
    }
  }

  if (root["brightness"].is<uint8_t>()) {
    call.set_brightness(float(root["brightness"]) / 255.0f);
  }

  if (root["color"].is<JsonObject>()) {
    JsonObject color = root["color"];
    // HA also encodes brightness information in the r, g, b values, so extract that and set it as color brightness.
    float max_rgb = 0.0f;
    if (color["r"].is<uint8_t>()) {
      float r = float(color["r"]) / 255.0f;
      max_rgb = fmaxf(max_rgb, r);
      call.set_red(r);
    }
    if (color["g"].is<uint8_t>()) {
      float g = float(color["g"]) / 255.0f;
      max_rgb = fmaxf(max_rgb, g);
      call.set_green(g);
    }
    if (color["b"].is<uint8_t>()) {
      float b = float(color["b"]) / 255.0f;
      max_rgb = fmaxf(max_rgb, b);
      call.set_blue(b);
    }
    if (color["r"].is<uint8_t>() || color["g"].is<uint8_t>() || color["b"].is<uint8_t>()) {
      call.set_color_brightness(max_rgb);
    }

    if (color["c"].is<uint8_t>()) {
      call.set_cold_white(float(color["c"]) / 255.0f);
    }
    if (color["w"].is<uint8_t>()) {
      // the HA scheme is ambiguous here, the same key is used for white channel in RGBW and warm
      // white channel in RGBWW.
      if (color["c"].is<uint8_t>()) {
        call.set_warm_white(float(color["w"]) / 255.0f);
      } else {
        call.set_white(float(color["w"]) / 255.0f);
      }
    }
  }

  if (root["white_value"].is<uint8_t>()) {  // legacy API
    call.set_white(float(root["white_value"]) / 255.0f);
  }

  if (root["color_temp"].is<uint16_t>()) {
    call.set_color_temperature(float(root["color_temp"]));
  }
}

void LightJSONSchema::parse_json(LightState &state, LightCall &call, JsonObject root) {
  LightJSONSchema::parse_color_json(state, call, root);

  if (root["flash"].is<uint32_t>()) {
    auto length = uint32_t(float(root["flash"]) * 1000);
    call.set_flash_length(length);
  }

  if (root["transition"].is<uint16_t>()) {
    auto length = uint32_t(float(root["transition"]) * 1000);
    call.set_transition_length(length);
  }

  if (root["effect"].is<const char *>()) {
    const char *effect = root["effect"];
    call.set_effect(effect);
  }

  if (root["effect_index"].is<uint32_t>()) {
    uint32_t effect_index = root["effect_index"];
    call.set_effect(effect_index);
  }
}

}  // namespace light
}  // namespace esphome

#endif
