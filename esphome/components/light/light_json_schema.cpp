#include "light_json_schema.h"
#include "light_output.h"

#ifdef USE_JSON

namespace esphome {
namespace light {

// See https://www.home-assistant.io/integrations/light.mqtt/#json-schema for documentation on the schema

void LightJSONSchema::dump_json(LightState &state, JsonObject &root) {
  if (state.supports_effects())
    root["effect"] = state.get_effect_name();

  auto values = state.remote_values;
  auto traits = state.get_output()->get_traits();

  switch (values.get_color_mode()) {
    case ColorMode::UNKNOWN:  // don't need to set color mode if we don't know it
      break;
    case ColorMode::ON_OFF:
      root["color_mode"] = "onoff";
      break;
    case ColorMode::BRIGHTNESS:
      root["color_mode"] = "brightness";
      break;
    case ColorMode::WHITE:  // not supported by HA in MQTT
      root["color_mode"] = "white";
      break;
    case ColorMode::COLOR_TEMPERATURE:
      root["color_mode"] = "color_temp";
      break;
    case ColorMode::COLD_WARM_WHITE:  // not supported by HA
      root["color_mode"] = "cwww";
      break;
    case ColorMode::RGB:
      root["color_mode"] = "rgb";
      break;
    case ColorMode::RGB_WHITE:
      root["color_mode"] = "rgbw";
      break;
    case ColorMode::RGB_COLOR_TEMPERATURE:  // not supported by HA
      root["color_mode"] = "rgbct";
      break;
    case ColorMode::RGB_COLD_WARM_WHITE:
      root["color_mode"] = "rgbww";
      break;
  }

  if (values.get_color_mode() & ColorCapability::ON_OFF)
    root["state"] = (values.get_state() != 0.0f) ? "ON" : "OFF";
  if (values.get_color_mode() & ColorCapability::BRIGHTNESS)
    root["brightness"] = uint8_t(values.get_brightness() * 255);

  JsonObject &color = root.createNestedObject("color");
  if (values.get_color_mode() & ColorCapability::RGB) {
    color["r"] = uint8_t(values.get_color_brightness() * values.get_red() * 255);
    color["g"] = uint8_t(values.get_color_brightness() * values.get_green() * 255);
    color["b"] = uint8_t(values.get_color_brightness() * values.get_blue() * 255);
  }
  if (values.get_color_mode() & ColorCapability::WHITE) {
    color["w"] = uint8_t(values.get_white() * 255);
    root["white_value"] = uint8_t(values.get_white() * 255);  // legacy API
  }
  if (values.get_color_mode() & ColorCapability::COLOR_TEMPERATURE) {
    // this one isn't under the color subkey for some reason
    root["color_temp"] = uint32_t(values.get_color_temperature());
  }
  if (values.get_color_mode() & ColorCapability::COLD_WARM_WHITE) {
    color["c"] = uint8_t(values.get_cold_white() * 255);
    color["w"] = uint8_t(values.get_warm_white() * 255);
  }
}

void LightJSONSchema::parse_color_json(LightState &state, LightCall &call, JsonObject &root) {
  if (root.containsKey("state")) {
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

  if (root.containsKey("brightness")) {
    call.set_brightness(float(root["brightness"]) / 255.0f);
  }

  if (root.containsKey("color")) {
    JsonObject &color = root["color"];
    // HA also encodes brightness information in the r, g, b values, so extract that and set it as color brightness.
    float max_rgb = 0.0f;
    if (color.containsKey("r")) {
      float r = float(color["r"]) / 255.0f;
      max_rgb = fmaxf(max_rgb, r);
      call.set_red(r);
    }
    if (color.containsKey("g")) {
      float g = float(color["g"]) / 255.0f;
      max_rgb = fmaxf(max_rgb, g);
      call.set_green(g);
    }
    if (color.containsKey("b")) {
      float b = float(color["b"]) / 255.0f;
      max_rgb = fmaxf(max_rgb, b);
      call.set_blue(b);
    }
    if (color.containsKey("r") || color.containsKey("g") || color.containsKey("b")) {
      call.set_color_brightness(max_rgb);
    }

    if (color.containsKey("c")) {
      call.set_cold_white(float(color["c"]) / 255.0f);
    }
    if (color.containsKey("w")) {
      // the HA scheme is ambigious here, the same key is used for white channel in RGBW and warm
      // white channel in RGBWW.
      if (color.containsKey("c")) {
        call.set_warm_white(float(color["w"]) / 255.0f);
      } else {
        call.set_white(float(color["w"]) / 255.0f);
      }
    }
  }

  if (root.containsKey("white_value")) {  // legacy API
    call.set_white(float(root["white_value"]) / 255.0f);
  }

  if (root.containsKey("color_temp")) {
    call.set_color_temperature(float(root["color_temp"]));
  }
}

void LightJSONSchema::parse_json(LightState &state, LightCall &call, JsonObject &root) {
  LightJSONSchema::parse_color_json(state, call, root);

  if (root.containsKey("flash")) {
    auto length = uint32_t(float(root["flash"]) * 1000);
    call.set_flash_length(length);
  }

  if (root.containsKey("transition")) {
    auto length = uint32_t(float(root["transition"]) * 1000);
    call.set_transition_length(length);
  }

  if (root.containsKey("effect")) {
    const char *effect = root["effect"];
    call.set_effect(effect);
  }
}

}  // namespace light
}  // namespace esphome

#endif
