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

// Home Assistant's color mode names, in flash on the ESP8266. Every color mode is handled as
// RGB: Home Assistant reports rgb_color for all of them and derives the white channels from
// rgb_color itself.
PROGMEM_STRING_TABLE(ColorModeNames, "onoff", "brightness", "white", "color_temp", "hs", "xy", "rgb", "rgbw", "rgbww");
static const uint8_t COLOR_MODES[] PROGMEM = {
    static_cast<uint8_t>(ColorMode::ON_OFF), static_cast<uint8_t>(ColorMode::BRIGHTNESS),
    static_cast<uint8_t>(ColorMode::WHITE),  static_cast<uint8_t>(ColorMode::COLOR_TEMPERATURE),
    static_cast<uint8_t>(ColorMode::RGB),    static_cast<uint8_t>(ColorMode::RGB),
    static_cast<uint8_t>(ColorMode::RGB),    static_cast<uint8_t>(ColorMode::RGB),
    static_cast<uint8_t>(ColorMode::RGB),
};
static_assert(sizeof(COLOR_MODES) == ColorModeNames::COUNT, "one color mode per name");

optional<ColorMode> parse_color_mode(StringRef name) {
  for (uint8_t i = 0; i < ColorModeNames::COUNT; i++) {
    const char *entry = ColorModeNames::get_(i, i);
    if (ESPHOME_strlen_P(entry) == name.size() && ESPHOME_strncmp_P(name.c_str(), entry, name.size()) == 0) {
      return static_cast<ColorMode>(progmem_read_byte(&COLOR_MODES[i]));
    }
  }
  return nullopt;
}

/// Reads the integers of a Python tuple such as "(255, 0, 0)"; false when there are fewer than `values`
template<size_t N> bool parse_tuple(const char *text, std::array<uint8_t, N> &values) {
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

  server->subscribe_home_assistant_state(this->entity_id_, nullptr, [this](StringRef value) {
    auto on_off = parse_on_off(value.c_str());
    if (on_off == PARSE_ON || on_off == PARSE_OFF) {
      this->import_(this->imported_.state, on_off == PARSE_ON);
    } else if (value != "None") {
      ESP_LOGW(TAG, "'%s': Can't parse '%s' as state!", this->entity_id_, value.c_str());
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "color_mode", [this](StringRef value) {
    if (auto mode = parse_color_mode(value)) {
      this->import_(this->imported_.color_mode, *mode);
    } else if (value != "None") {
      ESP_LOGE(TAG, "'%s': Can't parse 'color_mode' value '%s' as color mode!", this->entity_id_, value.c_str());
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "brightness", [this](StringRef value) {
    if (auto brightness = this->parse_attribute_("brightness", value)) {
      this->import_(this->imported_.brightness, *brightness / 255.0f);
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "color_temp_kelvin", [this](StringRef value) {
    if (auto mireds = this->parse_kelvin_("color_temp_kelvin", value)) {
      this->import_(this->imported_.color_temp, *mireds);
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "rgb_color", [this](StringRef value) {
    std::array<uint8_t, 3> rgb;
    if (parse_tuple(value.c_str(), rgb)) {
      this->import_(this->imported_.rgb, rgb);
    } else if (value != "None") {
      ESP_LOGE(TAG, "'%s': Can't parse 'rgb_color' value '%s'!", this->entity_id_, value.c_str());
    }
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

template<typename T> void HomeassistantLight::import_(optional<T> &field, T value) {
  field = value;
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
    call.set_color_mode(imported.color_mode);
    call.set_brightness(imported.brightness);
    // Home Assistant reports rgb_color in every mode and keeps the attributes of the other
    // modes, so only the ones of the mode it reports are applied
    const ColorMode mode = imported.color_mode.value_or(this->state_->remote_values.get_color_mode());
    if (mode == ColorMode::COLOR_TEMPERATURE) {
      call.set_color_temperature(imported.color_temp);
    } else if (mode == ColorMode::RGB) {
      if (imported.rgb.has_value()) {
        const auto &rgb = *imported.rgb;
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
    ESP_LOGW(TAG, "'%s': No entity ID set yet", this->state_->get_name().c_str());
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
      auto &entry = resp.data.emplace_back();
      entry.key = color_mode == ColorMode::WHITE ? WHITE_KEY : BRIGHTNESS_KEY;
      entry.value = StringRef(level_buf, buf_append_printf(level_buf, sizeof(level_buf), 0, "%u",
                                                           static_cast<unsigned>(level * 255.0f + 0.5f)));
    }
    if (color_mode == ColorMode::COLOR_TEMPERATURE && values.get_color_temperature() > 0.0f) {
      auto &color_temp = resp.data.emplace_back();
      color_temp.key = COLOR_TEMP_KEY;
      color_temp.value = StringRef(
          color_temp_buf, buf_append_printf(color_temp_buf, sizeof(color_temp_buf), 0, "%u",
                                            static_cast<unsigned>(1e6f / values.get_color_temperature() + 0.5f)));
    }
    if (color_mode == ColorMode::RGB) {
      // A list, which only a template renders as one
      resp.data_template.init(1);
      auto &color = resp.data_template.emplace_back();
      color.key = RGB_COLOR_KEY;
      color.value = StringRef(color_buf, buf_append_printf(color_buf, sizeof(color_buf), 0, "[%u,%u,%u]",
                                                           static_cast<unsigned>(values.get_red() * 255.0f + 0.5f),
                                                           static_cast<unsigned>(values.get_green() * 255.0f + 0.5f),
                                                           static_cast<unsigned>(values.get_blue() * 255.0f + 0.5f)));
    }
  }
  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
