#include "homeassistant_light.h"

#include <cstdlib>
#include <cstring>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
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
static const uint8_t COLOR_MODES[] PROGMEM = {
    static_cast<uint8_t>(ColorMode::ON_OFF),
    static_cast<uint8_t>(ColorMode::BRIGHTNESS),
    static_cast<uint8_t>(ColorMode::WHITE),
    static_cast<uint8_t>(ColorMode::COLOR_TEMPERATURE),
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

/// Reads the integers of a Python tuple such as "(255, 0, 0)"; false when there are fewer than three
bool parse_rgb(const char *text, std::array<uint8_t, 3> &values) {
  for (auto &value : values) {
    while (*text != '\0' && (*text < '0' || *text > '9')) {
      text++;
    }
    if (*text == '\0') {
      return false;
    }
    value = static_cast<uint8_t>(std::min(strtol(text, const_cast<char **>(&text), 10), 255L));
  }
  return true;
}

}  // namespace

void HomeassistantLight::setup() {
  auto *server = api::global_api_server;
  server->get_home_assistant_state(this->entity_id_, "supported_color_modes", [this](StringRef value) {
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
    ESP_LOGD(TAG, "'%s': Supported color modes retrieved: %s", this->entity_id_, value.c_str());
    this->traits_.set_supported_color_modes(modes);
  });
  // Home Assistant reports the temperature range in Kelvin, the light core works in mireds
  server->get_home_assistant_state(this->entity_id_, "min_color_temp_kelvin", [this](StringRef value) {
    if (auto mireds = this->parse_kelvin_("min_color_temp_kelvin", value)) {
      this->traits_.set_max_mireds(*mireds);
    }
  });
  server->get_home_assistant_state(this->entity_id_, "max_color_temp_kelvin", [this](StringRef value) {
    if (auto mireds = this->parse_kelvin_("max_color_temp_kelvin", value)) {
      this->traits_.set_min_mireds(*mireds);
    }
  });

  // An attribute is "None" while the light is off or does not report it; the last value stays
  server->subscribe_home_assistant_state(this->entity_id_, nullptr, [this](StringRef value) {
    if (value == "None") {
      return;
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
    if (value == "None") {
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
    if (auto kelvin = this->parse_attribute_("color_temp_kelvin", value)) {
      this->imported_.color_temp_kelvin = static_cast<uint16_t>(clamp(*kelvin, 1.0f, 65535.0f));
      this->schedule_apply_();
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "rgb_color", [this](StringRef value) {
    if (value == "None") {
      return;
    }
    if (!parse_rgb(value.c_str(), this->imported_.rgb)) {
      ESP_LOGE(TAG, "'%s': Can't parse 'rgb_color' value '%s'!", this->entity_id_, value.c_str());
      return;
    }
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
  if (value == "None") {
    return nullopt;
  }
  auto number = parse_number<float>(value.c_str());
  if (!number.has_value()) {
    ESP_LOGE(TAG, "'%s': Can't convert '%s' value '%s' to number!", this->entity_id_, name, value.c_str());
  }
  return number;
}

optional<float> HomeassistantLight::parse_kelvin_(const char *name, StringRef value) {
  auto kelvin = this->parse_attribute_(name, value);
  if (kelvin.has_value() && *kelvin > 0.0f) {
    return 1e6f / *kelvin;
  }
  return nullopt;
}

void HomeassistantLight::schedule_apply_() {
  // One update changes several attributes, each arriving as its own message; apply them as one call
  if (!this->apply_scheduled_) {
    this->apply_scheduled_ = true;
    this->defer([this]() { this->apply_imported_(); });
  }
}

void HomeassistantLight::apply_imported_() {
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
  if (this->remote_values_version_ == this->import_version_) {
    return;  // this change came from Home Assistant
  }
  if (!api::global_api_server->is_connected()) {
    ESP_LOGE(TAG, "No clients connected to API server");
    return;
  }
#ifdef USE_API_WIZARD_LINKED_INPUTS
  if (this->entity_id_[0] == '\0') {
    ESP_LOGW(TAG, "'%s': No entity ID set yet", state->get_name().c_str());
    return;
  }
#endif

  static constexpr auto SERVICE_ON = StringRef::from_lit("light.turn_on");
  static constexpr auto SERVICE_OFF = StringRef::from_lit("light.turn_off");
  static constexpr auto ENTITY_ID_KEY = StringRef::from_lit("entity_id");
  static constexpr auto BRIGHTNESS_KEY = StringRef::from_lit("brightness");
  static constexpr auto WHITE_KEY = StringRef::from_lit("white");
  static constexpr auto COLOR_TEMP_KEY = StringRef::from_lit("color_temp_kelvin");
  static constexpr auto RGB_COLOR_KEY = StringRef::from_lit("rgb_color");

  api::HomeassistantActionRequest resp;
  resp.data.init(3);
  auto &entity_id = resp.data.emplace_back();
  entity_id.key = ENTITY_ID_KEY;
  entity_id.value = StringRef(this->entity_id_);

  // The request refers to these buffers until it is sent below
  char level_buf[UINT32_MAX_STR_SIZE];
  char color_temp_buf[UINT32_MAX_STR_SIZE];
  char color_buf[16];
  auto add_number = [&resp](StringRef key, char *buf, size_t size, unsigned number) {
    auto &entry = resp.data.emplace_back();
    entry.key = key;
    entry.value = StringRef(buf, buf_append_printf(buf, size, 0, "%u", number));
  };
  if (!values.is_on()) {
    resp.service = SERVICE_OFF;
  } else {
    resp.service = SERVICE_ON;
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
      add_number(color_mode == ColorMode::WHITE ? WHITE_KEY : BRIGHTNESS_KEY, level_buf, sizeof(level_buf),
                 to_uint8_scale(level));
    }
    if (color_mode == ColorMode::COLOR_TEMPERATURE && values.get_color_temperature() > 0.0f) {
      add_number(COLOR_TEMP_KEY, color_temp_buf, sizeof(color_temp_buf),
                 static_cast<unsigned>(lroundf(values.get_color_temperature_kelvin())));
    }
    if (color_mode == ColorMode::RGB) {
      // A list, which only a template renders as one
      resp.data_template.init(1);
      auto &color = resp.data_template.emplace_back();
      color.key = RGB_COLOR_KEY;
      color.value = StringRef(
          color_buf, buf_append_printf(color_buf, sizeof(color_buf), 0, "[%u,%u,%u]", to_uint8_scale(values.get_red()),
                                       to_uint8_scale(values.get_green()), to_uint8_scale(values.get_blue())));
    }
  }
  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
