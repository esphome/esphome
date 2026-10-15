#include "homeassistant_light.h"

#include <cstring>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "../homeassistant.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.light");

static constexpr uint32_t ECHO_TIMEOUT_ID = 0;
static constexpr uint32_t ECHO_TIMEOUT_MS = 2000;  // Home Assistant answers an action well within this

using namespace esphome::light;

namespace {

// Home Assistant's color mode names, in flash on the ESP8266. The names from "hs" on are every
// color mode, all handled as RGB: Home Assistant reports rgb_color for all of them and derives
// the white channels from rgb_color itself.
PROGMEM_STRING_TABLE(ColorModeNames, "onoff", "brightness", "white", "color_temp", "hs", "xy", "rgb", "rgbw", "rgbww");
const uint8_t COLOR_MODES[] PROGMEM = {
    static_cast<uint8_t>(ColorMode::ON_OFF),
    static_cast<uint8_t>(ColorMode::BRIGHTNESS),
    static_cast<uint8_t>(ColorMode::WHITE),
    static_cast<uint8_t>(ColorMode::COLOR_TEMPERATURE),
};

// The service and data names of the actions, in flash on the ESP8266; ActionString indexes them
PROGMEM_STRING_TABLE(ActionStrings, "light.turn_on", "light.turn_off", "entity_id", "brightness", "white",
                     "color_temp_kelvin", "rgb_color");
enum ActionString : uint8_t {
  ACTION_STRING_TURN_ON,
  ACTION_STRING_TURN_OFF,
  ACTION_STRING_ENTITY_ID,
  ACTION_STRING_BRIGHTNESS,
  ACTION_STRING_WHITE,
  ACTION_STRING_COLOR_TEMP_KELVIN,
  ACTION_STRING_RGB_COLOR,
};

optional<ColorMode> parse_color_mode(StringRef name) {
  const int8_t i = progmem_string_table_find<ColorModeNames>(name.c_str(), name.size());
  if (i < 0) {
    return nullopt;
  }
  return i < sizeof(COLOR_MODES) ? static_cast<ColorMode>(progmem_read_byte(&COLOR_MODES[i])) : ColorMode::RGB;
}

/// Reads a Python tuple of three integers such as "(255, 0, 0)"; false for anything else
bool parse_rgb(const char *text, std::array<uint8_t, 3> &values) {
  for (auto &value : values) {
    while (*text == ' ' || *text == ',' || *text == '(') {
      text++;
    }
    if (*text < '0' || *text > '9') {
      return false;
    }
    uint16_t number = 0;
    while (*text >= '0' && *text <= '9') {
      number = number * 10 + (*text++ - '0');
      if (number > 255) {
        return false;
      }
    }
    value = static_cast<uint8_t>(number);
  }
  while (*text == ' ' || *text == ',' || *text == ')') {
    text++;
  }
  return *text == '\0';
}

}  // namespace

void HomeassistantLight::setup() {
  auto *server = api::global_api_server;
  // Home Assistant repeats these with every update, so a light that comes up late still gets them
  server->subscribe_home_assistant_state(this->entity_id_, "supported_color_modes", [this](StringRef value) {
    if (is_unset(value)) {
      return;  // the light is unavailable; its traits stay
    }
    // A Python list of the mode names, as strings or as the enum members
    ColorModeMask modes;
    char name[ColorModeNames::MAX_LENGTH + 1];
    size_t length = 0;
    const bool valid = parse_python_list(
        value,
        [&](char c) {
          if (length < sizeof(name)) {
            name[length] = c;
          }
          length++;
        },
        [&]() {
          auto mode = length <= ColorModeNames::MAX_LENGTH ? parse_color_mode(StringRef(name, length)) : nullopt;
          if (mode.has_value()) {
            modes.insert(*mode);
          } else {
            ESP_LOGW(TAG, "'%s': Unknown color mode in '%s'", this->entity_id_, value.c_str());
          }
          length = 0;
        });
    if (!valid) {
      ESP_LOGW(TAG, "'%s': Can't parse 'supported_color_modes' value '%s'", this->entity_id_, value.c_str());
      return;
    }
    if (modes.empty()) {
      ESP_LOGW(TAG, "'%s': No color mode found in '%s', keeping the current ones", this->entity_id_, value.c_str());
      return;
    }
    ESP_LOGD(TAG, "'%s': Supported color modes retrieved: %s", this->entity_id_, value.c_str());
    this->traits_.set_supported_color_modes(modes);
    // Attributes that arrived before the traits were validated against empty ones; apply them again
    this->schedule_apply_();
  });
  // Home Assistant reports the temperature range in Kelvin, the light core works in mireds
  server->subscribe_home_assistant_state(this->entity_id_, "min_color_temp_kelvin", [this](StringRef value) {
    if (auto kelvin = this->parse_integer_("min_color_temp_kelvin", value, UINT16_MAX)) {
      this->traits_.set_max_mireds(1e6f / *kelvin);
      this->schedule_apply_();
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "max_color_temp_kelvin", [this](StringRef value) {
    if (auto kelvin = this->parse_integer_("max_color_temp_kelvin", value, UINT16_MAX)) {
      this->traits_.set_min_mireds(1e6f / *kelvin);
      this->schedule_apply_();
    }
  });

  // An attribute is "None" while the light is off or does not report it; the last value stays
  server->subscribe_home_assistant_state(this->entity_id_, nullptr, [this](StringRef value) {
    if (is_unset(value)) {
      return;  // normal while Home Assistant or the light is starting up or offline
    }
    auto on_off = parse_on_off(value.c_str());
    if (on_off != PARSE_ON && on_off != PARSE_OFF) {
      ESP_LOGW(TAG, "'%s': Can't parse '%s' as state!", this->entity_id_, value.c_str());
      return;
    }
    this->imported_.state = on_off == PARSE_ON;
    this->schedule_apply_();
  });
  server->subscribe_home_assistant_state(this->entity_id_, "color_mode", [this](StringRef value) {
    if (is_unset(value)) {
      return;
    }
    auto mode = parse_color_mode(value);
    if (!mode.has_value()) {
      ESP_LOGE(TAG, "'%s': Can't parse 'color_mode' value '%s' as color mode!", this->entity_id_, value.c_str());
      return;
    }
    this->imported_.color_mode = *mode;
    this->schedule_apply_();
  });
  server->subscribe_home_assistant_state(this->entity_id_, "brightness", [this](StringRef value) {
    if (auto brightness = this->parse_integer_("brightness", value, 255)) {
      this->imported_.brightness = *brightness;
      this->schedule_apply_();
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "color_temp_kelvin", [this](StringRef value) {
    if (auto kelvin = this->parse_integer_("color_temp_kelvin", value, UINT16_MAX)) {
      this->imported_.color_temp_kelvin = *kelvin;
      this->schedule_apply_();
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "rgb_color", [this](StringRef value) {
    if (is_unset(value)) {
      return;
    }
    std::array<uint8_t, 3> rgb;
    if (!parse_rgb(value.c_str(), rgb)) {
      ESP_LOGE(TAG, "'%s': Can't parse 'rgb_color' value '%s'!", this->entity_id_, value.c_str());
      return;
    }
    this->imported_.rgb = rgb;
    this->imported_.has_rgb = true;
    this->schedule_apply_();
  });
}

void HomeassistantLight::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Homeassistant Light:\n"
                "  Entity ID: '%s'",
                this->entity_id_);
}

optional<uint16_t> HomeassistantLight::parse_integer_(const char *name, StringRef value, uint16_t max) {
  if (is_unset(value)) {
    return nullopt;
  }
  auto number = parse_number<float>(value.c_str());
  if (!number.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert '%s' value '%s' to number!", this->entity_id_, name, value.c_str());
    return nullopt;
  }
  // Written so that NaN fails too
  if (!(*number >= 1.0f && *number <= max)) {
    ESP_LOGW(TAG, "'%s': '%s' value %.0f is out of range!", this->entity_id_, name, *number);
    return nullopt;
  }
  return static_cast<uint16_t>(*number);
}

void HomeassistantLight::schedule_apply_() {
  // A fresh report makes the one that waited for a local change moot
  this->cancel_timeout(ECHO_TIMEOUT_ID);
  // One update changes several attributes, each arriving as its own message; apply them as one call
  if (!this->apply_scheduled_) {
    this->apply_scheduled_ = true;
    this->defer([this]() { this->apply_imported_(); });
  }
}

void HomeassistantLight::apply_imported_() {
  if (!this->imported_.state.has_value()) {
    this->apply_scheduled_ = false;
    return;  // the traits came in before any state
  }
  // Attributes Home Assistant has not reported keep what the light has
  const Attributes &imported = this->imported_;
  LightColorValues values = this->state_->remote_values;
  values.set_state(*imported.state);
  if (*imported.state) {
    if (imported.color_mode != ColorMode::UNKNOWN) {
      values.set_color_mode(imported.color_mode);
    }
    if (imported.brightness != 0) {
      values.set_brightness(imported.brightness / 255.0f);
    }
    // Home Assistant reports rgb_color in every mode and keeps the attributes of the other
    // modes, so only the ones of the mode it reports are taken over
    const ColorMode mode = values.get_color_mode();
    if (mode == ColorMode::COLOR_TEMPERATURE) {
      values.set_color_temperature_kelvin(imported.color_temp_kelvin);  // ignores an unreported 0
    } else if (mode == ColorMode::RGB) {
      if (imported.has_rgb) {
        values.set_red(imported.rgb[0] / 255.0f);
        values.set_green(imported.rgb[1] / 255.0f);
        values.set_blue(imported.rgb[2] / 255.0f);
      }
      values.set_color_brightness(1.0f);  // brightness is the only level Home Assistant has
    } else if (mode == ColorMode::WHITE) {
      values.set_white(1.0f);
    }
  }
  this->apply_scheduled_ = false;
  if (!this->state_->set_external_state(values)) {
    // A local change still waits for LightState::loop() to send it, or a local transition or flash
    // runs. Home Assistant reports again once that change reaches it; when it does not, the echo
    // timeout write_state() arms brings the light back to what was reported last
    ESP_LOGD(TAG, "'%s': Report dropped, a local change is on its way", this->entity_id_);
    return;
  }
  ESP_LOGD(TAG, "'%s': Took over the state reported by Home Assistant", this->entity_id_);
}

void HomeassistantLight::write_state(LightState *state) {
  // A reported state is taken over without a write, so every write is a local change, except
  // the very first: the light core writes its initial state, which Home Assistant does not need
  if (this->initial_write_) {
    this->initial_write_ = false;
    return;
  }
  if (state->is_transitioning()) {
    return;  // Home Assistant gets the target once it is reached
  }
  const LightColorValues &values = state->current_values;
  if (!api::global_api_server->is_connected()) {
    ESP_LOGE(TAG, "'%s': No clients connected to API server, change lost%s", this->entity_id_,
             this->imported_.state.has_value() ? LOG_STR_LITERAL("")
                                               : LOG_STR_LITERAL(" and no reported state to fall back to"));
    this->schedule_apply_();  // back to the state Home Assistant reported, which the light still has
    return;
  }
#ifdef USE_API_WIZARD_LINKED_INPUTS
  if (this->entity_id_[0] == '\0') {
    ESP_LOGW(TAG, "'%s': No entity ID set yet", state->get_name().c_str());
    this->schedule_apply_();
    return;
  }
#endif

  // The request refers to these strings and buffers until it is sent below
  ProgmemStringTableCopy<ActionStrings> strings;
  char level_buf[4];       // up to 255
  char color_temp_buf[6];  // up to 65535
  char color_buf[16];

  api::HomeassistantActionRequest resp;
  resp.data.init(3);
  auto &entity_id = resp.data.emplace_back();
  entity_id.key = StringRef(strings.get(ACTION_STRING_ENTITY_ID));
  entity_id.value = StringRef(this->entity_id_);

  auto add_number = [&](uint8_t key, auto &buf, unsigned number) {
    auto &entry = resp.data.emplace_back();
    entry.key = StringRef(strings.get(key));
    entry.value = StringRef(buf, buf_append_printf(buf, sizeof(buf), 0, "%u", number));
  };
  if (!values.is_on()) {
    resp.service = StringRef(strings.get(ACTION_STRING_TURN_OFF));
  } else {
    resp.service = StringRef(strings.get(ACTION_STRING_TURN_ON));
    const ColorMode color_mode = values.get_color_mode();
    if (color_mode != ColorMode::ON_OFF) {
      // Home Assistant has one level, so the channel brightness of the mode folds into it. In
      // white mode it goes under "white", which also selects that mode
      float level = values.get_brightness();
      uint8_t key = ACTION_STRING_BRIGHTNESS;
      if (color_mode == ColorMode::RGB) {
        level *= values.get_color_brightness();
      } else if (color_mode == ColorMode::WHITE) {
        level *= values.get_white();
        key = ACTION_STRING_WHITE;
      }
      add_number(key, level_buf, to_uint8_scale(level));
    }
    if (color_mode == ColorMode::COLOR_TEMPERATURE && values.get_color_temperature() > 0.0f) {
      add_number(ACTION_STRING_COLOR_TEMP_KELVIN, color_temp_buf,
                 static_cast<unsigned>(lroundf(values.get_color_temperature_kelvin())));
    }
    if (color_mode == ColorMode::RGB) {
      // A list, which only a template renders as one
      resp.data_template.init(1);
      auto &color = resp.data_template.emplace_back();
      color.key = StringRef(strings.get(ACTION_STRING_RGB_COLOR));
      color.value = StringRef(
          color_buf, buf_append_printf(color_buf, sizeof(color_buf), 0, "[%u,%u,%u]", to_uint8_scale(values.get_red()),
                                       to_uint8_scale(values.get_green()), to_uint8_scale(values.get_blue())));
    }
  }
  api::global_api_server->send_homeassistant_action(resp);
  // Home Assistant reports the change back in a moment, which cancels this; when it does not, the
  // change got lost on the way (no subscriber yet, the entity unavailable, the action refused) and
  // the light goes back to what was reported last
  this->set_timeout(ECHO_TIMEOUT_ID, ECHO_TIMEOUT_MS, [this]() {
    ESP_LOGW(TAG, "'%s': Home Assistant did not report the change back, falling back to its last state",
             this->entity_id_);
    this->schedule_apply_();
  });
}

}  // namespace esphome::homeassistant
