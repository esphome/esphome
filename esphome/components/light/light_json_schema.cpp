#include "light_json_schema.h"
#include "light_output.h"
#include "esphome/core/progmem.h"

#ifdef USE_JSON

namespace esphome::light {

// See https://www.home-assistant.io/integrations/light.mqtt/#json-schema for documentation on the schema

// Get JSON string for color mode using linear search (avoids large switch jump table)
static const char *get_color_mode_json_str(ColorMode mode) {
  // Parallel arrays: mode values and their corresponding strings
  // Uses less RAM than a switch jump table on sparse enum values
  static constexpr ColorMode MODES[] = {
      ColorMode::ON_OFF,
      ColorMode::BRIGHTNESS,
      ColorMode::WHITE,
      ColorMode::COLOR_TEMPERATURE,
      ColorMode::COLD_WARM_WHITE,
      ColorMode::RGB,
      ColorMode::RGB_WHITE,
      ColorMode::RGB_COLOR_TEMPERATURE,
      ColorMode::RGB_COLD_WARM_WHITE,
  };
  static constexpr const char *STRINGS[] = {
      "onoff", "brightness", "white", "color_temp", "cwww", "rgb", "rgbw", "rgbct", "rgbww",
  };
  for (size_t i = 0; i < sizeof(MODES) / sizeof(MODES[0]); i++) {
    if (MODES[i] == mode)
      return STRINGS[i];
  }
  return nullptr;
}

void LightJSONSchema::dump_json(LightState &state, JsonObject root) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  if (state.supports_effects()) {
    root[ESPHOME_F("effect")] = state.get_effect_name();
    root[ESPHOME_F("effect_index")] = state.get_current_effect_index();
    root[ESPHOME_F("effect_count")] = state.get_effect_count();
  }

  auto values = state.remote_values;

  const auto color_mode = values.get_color_mode();
  const char *mode_str = get_color_mode_json_str(color_mode);
  if (mode_str != nullptr) {
    root[ESPHOME_F("color_mode")] = mode_str;
  }

  if (color_mode & ColorCapability::ON_OFF)
    root[ESPHOME_F("state")] = (values.get_state() != 0.0f) ? "ON" : "OFF";
  if (color_mode & ColorCapability::BRIGHTNESS)
    root[ESPHOME_F("brightness")] = to_uint8_scale(values.get_brightness());

  JsonObject color = root[ESPHOME_F("color")].to<JsonObject>();
  if (color_mode & ColorCapability::RGB) {
    float color_brightness = values.get_color_brightness();
    color[ESPHOME_F("r")] = to_uint8_scale(color_brightness * values.get_red());
    color[ESPHOME_F("g")] = to_uint8_scale(color_brightness * values.get_green());
    color[ESPHOME_F("b")] = to_uint8_scale(color_brightness * values.get_blue());
  }
  if (color_mode & ColorCapability::WHITE) {
    uint8_t white_val = to_uint8_scale(values.get_white());
    color[ESPHOME_F("w")] = white_val;
    root[ESPHOME_F("white_value")] = white_val;  // legacy API
  }
  if (color_mode & ColorCapability::COLOR_TEMPERATURE) {
    // this one isn't under the color subkey for some reason
    root[ESPHOME_F("color_temp")] = uint32_t(values.get_color_temperature());
  }
  if (color_mode & ColorCapability::COLD_WARM_WHITE) {
    color[ESPHOME_F("c")] = to_uint8_scale(values.get_cold_white());
    color[ESPHOME_F("w")] = to_uint8_scale(values.get_warm_white());
  }
}

void LightJSONSchema::parse_color_json(LightState &state, LightCall &call, JsonObject root) {
  if (root[ESPHOME_F("state")].is<const char *>()) {
    auto val = parse_on_off(root[ESPHOME_F("state")]);
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

  if (root[ESPHOME_F("brightness")].is<uint8_t>()) {
    call.set_brightness(float(root[ESPHOME_F("brightness")]) / 255.0f);
  }

  if (root[ESPHOME_F("color")].is<JsonObject>()) {
    JsonObject color = root[ESPHOME_F("color")];
    // HA also encodes brightness information in the r, g, b values, so extract that and set it as color brightness.
    float max_rgb = 0.0f;
    if (color[ESPHOME_F("r")].is<uint8_t>()) {
      float r = float(color[ESPHOME_F("r")]) / 255.0f;
      max_rgb = fmaxf(max_rgb, r);
      call.set_red(r);
    }
    if (color[ESPHOME_F("g")].is<uint8_t>()) {
      float g = float(color[ESPHOME_F("g")]) / 255.0f;
      max_rgb = fmaxf(max_rgb, g);
      call.set_green(g);
    }
    if (color[ESPHOME_F("b")].is<uint8_t>()) {
      float b = float(color[ESPHOME_F("b")]) / 255.0f;
      max_rgb = fmaxf(max_rgb, b);
      call.set_blue(b);
    }
    if (color[ESPHOME_F("r")].is<uint8_t>() || color[ESPHOME_F("g")].is<uint8_t>() ||
        color[ESPHOME_F("b")].is<uint8_t>()) {
      call.set_color_brightness(max_rgb);
    }

    if (color[ESPHOME_F("c")].is<uint8_t>()) {
      call.set_cold_white(float(color[ESPHOME_F("c")]) / 255.0f);
    }
    if (color[ESPHOME_F("w")].is<uint8_t>()) {
      // the HA scheme is ambiguous here, the same key is used for white channel in RGBW and warm
      // white channel in RGBWW.
      if (color[ESPHOME_F("c")].is<uint8_t>()) {
        call.set_warm_white(float(color[ESPHOME_F("w")]) / 255.0f);
      } else {
        call.set_white(float(color[ESPHOME_F("w")]) / 255.0f);
      }
    }
  }

  if (root[ESPHOME_F("white_value")].is<uint8_t>()) {  // legacy API
    call.set_white(float(root[ESPHOME_F("white_value")]) / 255.0f);
  }

  if (root[ESPHOME_F("color_temp")].is<uint16_t>()) {
    call.set_color_temperature(float(root[ESPHOME_F("color_temp")]));
  }
}

void LightJSONSchema::parse_json(LightState &state, LightCall &call, JsonObject root) {
  LightJSONSchema::parse_color_json(state, call, root);

  if (root[ESPHOME_F("flash")].is<uint32_t>()) {
    auto length = uint32_t(float(root[ESPHOME_F("flash")]) * 1000);
    call.set_flash_length(length);
  }

  if (root[ESPHOME_F("transition")].is<uint16_t>()) {
    auto length = uint32_t(float(root[ESPHOME_F("transition")]) * 1000);
    call.set_transition_length(length);
  }

  if (root[ESPHOME_F("effect")].is<const char *>()) {
    const char *effect = root[ESPHOME_F("effect")];
    call.set_effect(effect);
  }

  if (root[ESPHOME_F("effect_index")].is<uint32_t>()) {
    uint32_t effect_index = root[ESPHOME_F("effect_index")];
    call.set_effect(effect_index);
  }
}

}  // namespace esphome::light

#endif
