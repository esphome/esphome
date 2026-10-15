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
  for (uint8_t i = 0; i < ColorModeNames::COUNT; i++) {
    const char *entry = ColorModeNames::get_(i, i);
    if (ESPHOME_strlen_P(entry) == name.size() && ESPHOME_strncmp_P(name.c_str(), entry, name.size()) == 0) {
      return i < sizeof(COLOR_MODES) ? static_cast<ColorMode>(progmem_read_byte(&COLOR_MODES[i])) : ColorMode::RGB;
    }
  }
  return nullopt;
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
    // A Python list, "['hs', 'color_temp']" or "[<ColorMode.HS: 'hs'>, ...]": the names are the quoted parts
    ColorModeMask modes;
    const char *text = value.c_str();
    while ((text = strchr(text, '\'')) != nullptr) {
      const char *name = ++text;
      text = strchr(name, '\'');
      if (text == nullptr) {
        break;
      }
      if (auto mode = parse_color_mode(StringRef(name, text - name))) {
        modes.insert(*mode);
      } else {
        ESP_LOGW(TAG, "'%s': Can't parse '%.*s' as color mode!", this->entity_id_, static_cast<int>(text - name), name);
      }
      text++;
    }
    if (modes.empty()) {
      ESP_LOGW(TAG, "'%s': No color mode found in '%s', keeping the current ones", this->entity_id_, value.c_str());
      return;
    }
    ESP_LOGD(TAG, "'%s': Supported color modes retrieved: %s", this->entity_id_, value.c_str());
    this->traits_.set_supported_color_modes(modes);
  });
  // Home Assistant reports the temperature range in Kelvin, the light core works in mireds
  server->subscribe_home_assistant_state(this->entity_id_, "min_color_temp_kelvin", [this](StringRef value) {
    if (auto kelvin = this->parse_kelvin_("min_color_temp_kelvin", value)) {
      this->traits_.set_max_mireds(1e6f / *kelvin);
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "max_color_temp_kelvin", [this](StringRef value) {
    if (auto kelvin = this->parse_kelvin_("max_color_temp_kelvin", value)) {
      this->traits_.set_min_mireds(1e6f / *kelvin);
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
    if (auto brightness = this->parse_attribute_("brightness", value)) {
      this->imported_.brightness = static_cast<uint8_t>(clamp(*brightness, 1.0f, 255.0f));
      this->schedule_apply_();
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "color_temp_kelvin", [this](StringRef value) {
    if (auto kelvin = this->parse_kelvin_("color_temp_kelvin", value)) {
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

optional<float> HomeassistantLight::parse_attribute_(const char *name, StringRef value) {
  if (is_unset(value)) {
    return nullopt;
  }
  auto number = parse_number<float>(value.c_str());
  if (!number.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert '%s' value '%s' to number!", this->entity_id_, name, value.c_str());
  }
  return number;
}

optional<uint16_t> HomeassistantLight::parse_kelvin_(const char *name, StringRef value) {
  auto kelvin = this->parse_attribute_(name, value);
  if (!kelvin.has_value()) {
    return nullopt;
  }
  if (*kelvin < 1.0f || *kelvin > UINT16_MAX) {
    ESP_LOGW(TAG, "'%s': '%s' value %.0f is out of range!", this->entity_id_, name, *kelvin);
    return nullopt;
  }
  return static_cast<uint16_t>(*kelvin);
}

void HomeassistantLight::schedule_apply_() {
  // One update changes several attributes, each arriving as its own message; apply them as one call
  if (!this->apply_scheduled_) {
    this->apply_scheduled_ = true;
    this->defer([this]() { this->apply_imported_(); });
  }
}

void HomeassistantLight::apply_imported_() {
  if (this->remote_values_version_ != this->written_version_) {
    // A local change still waits for LightState::loop() to send it; write_state() picks the report up after it
    this->import_waiting_ = true;
    return;
  }
  this->apply_scheduled_ = false;
  const Attributes &imported = this->imported_;
  auto call = this->state_->make_call().set_save(false);
  call.set_state(imported.state);
  if (imported.state.value_or(true)) {
    if (imported.color_mode != ColorMode::UNKNOWN) {
      call.set_color_mode(imported.color_mode);
    }
    if (imported.brightness != 0) {
      call.set_brightness(imported.brightness / 255.0f);
    }
    // Home Assistant reports rgb_color in every mode and keeps the attributes of the other
    // modes, so only the ones of the mode it reports are applied
    const ColorMode mode =
        imported.color_mode != ColorMode::UNKNOWN ? imported.color_mode : this->state_->remote_values.get_color_mode();
    if (mode == ColorMode::COLOR_TEMPERATURE) {
      if (imported.color_temp_kelvin != 0) {
        call.set_color_temperature(1e6f / imported.color_temp_kelvin);
      }
    } else if (mode == ColorMode::RGB) {
      if (imported.has_rgb) {
        const auto &rgb = imported.rgb;
        call.set_rgb(rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f);
      }
      call.set_color_brightness(1.0f);  // brightness is the only level Home Assistant has
    } else if (mode == ColorMode::WHITE) {
      call.set_white(1.0f);
    }
  }
  ESP_LOGD(TAG, "'%s': Applying the state reported by Home Assistant", this->entity_id_);
  this->import_version_ = this->remote_values_version_ + 1;  // the version perform() publishes
  call.perform();
}

void HomeassistantLight::write_state(LightState *state) {
  const LightColorValues &values = state->current_values;
  if (values != state->remote_values) {
    return;  // a transition runs here; Home Assistant gets the target once it is reached
  }
  this->written_version_ = this->remote_values_version_;
  if (this->import_waiting_) {
    this->import_waiting_ = false;
    this->defer([this]() { this->apply_imported_(); });
  }
  if (this->remote_values_version_ == this->import_version_) {
    return;  // this change came from Home Assistant
  }
  if (!api::global_api_server->is_connected()) {
    ESP_LOGE(TAG, "'%s': No clients connected to API server, change lost", this->entity_id_);
    return;
  }
#ifdef USE_API_WIZARD_LINKED_INPUTS
  if (this->entity_id_[0] == '\0') {
    ESP_LOGW(TAG, "'%s': No entity ID set yet", state->get_name().c_str());
    return;
  }
#endif

  // The request refers to these strings and buffers until it is sent below
  ProgmemStringTableCopy<ActionStrings> strings;
  char level_buf[UINT32_MAX_STR_SIZE];
  char color_temp_buf[UINT32_MAX_STR_SIZE];
  char color_buf[16];

  api::HomeassistantActionRequest resp;
  resp.data.init(3);
  auto &entity_id = resp.data.emplace_back();
  entity_id.key = StringRef(strings.get(ACTION_STRING_ENTITY_ID));
  entity_id.value = StringRef(this->entity_id_);

  auto add_number = [&](uint8_t key, char *buf, size_t size, unsigned number) {
    auto &entry = resp.data.emplace_back();
    entry.key = StringRef(strings.get(key));
    entry.value = StringRef(buf, buf_append_printf(buf, size, 0, "%u", number));
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
      if (color_mode == ColorMode::RGB) {
        level *= values.get_color_brightness();
      } else if (color_mode == ColorMode::WHITE) {
        level *= values.get_white();
      }
      add_number(color_mode == ColorMode::WHITE ? ACTION_STRING_WHITE : ACTION_STRING_BRIGHTNESS, level_buf,
                 sizeof(level_buf), to_uint8_scale(level));
    }
    if (color_mode == ColorMode::COLOR_TEMPERATURE && values.get_color_temperature() > 0.0f) {
      add_number(ACTION_STRING_COLOR_TEMP_KELVIN, color_temp_buf, sizeof(color_temp_buf),
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
}

}  // namespace esphome::homeassistant
