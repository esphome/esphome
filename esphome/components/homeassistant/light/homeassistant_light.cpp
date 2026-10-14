#include "homeassistant_light.h"

#include <cstdlib>
#include <cstring>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.light");

using namespace esphome::light;

namespace {

struct ColorModeName {
  const char *name;
  ColorMode mode;
};

// Home Assistant's color mode names. Every color mode is handled as RGB: Home Assistant reports
// rgb_color for all of them and derives the white channels from rgb_color itself.
constexpr ColorModeName COLOR_MODE_NAMES[] = {
    {"onoff", ColorMode::ON_OFF}, {"brightness", ColorMode::BRIGHTNESS},
    {"white", ColorMode::WHITE},  {"color_temp", ColorMode::COLOR_TEMPERATURE},
    {"hs", ColorMode::RGB},       {"xy", ColorMode::RGB},
    {"rgb", ColorMode::RGB},      {"rgbw", ColorMode::RGB},
    {"rgbww", ColorMode::RGB},
};

optional<ColorMode> parse_color_mode(StringRef name) {
  for (const auto &entry : COLOR_MODE_NAMES) {
    if (name == entry.name) {
      return entry.mode;
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
      if ((text = strchr(name, '\'')) == nullptr) {
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
  server->get_home_assistant_state(this->entity_id_, "min_mireds", [this](StringRef value) {
    if (auto mireds = this->parse_attribute_("min_mireds", value)) {
      this->traits_.set_min_mireds(*mireds);
    }
  });
  server->get_home_assistant_state(this->entity_id_, "max_mireds", [this](StringRef value) {
    if (auto mireds = this->parse_attribute_("max_mireds", value)) {
      this->traits_.set_max_mireds(*mireds);
    }
  });

  server->subscribe_home_assistant_state(this->entity_id_, nullptr, [this](StringRef value) {
    auto on_off = parse_on_off(value.c_str());
    if (on_off == PARSE_ON || on_off == PARSE_OFF) {
      this->import_(this->pending_.state, on_off == PARSE_ON);
    } else if (value != "None") {
      ESP_LOGW(TAG, "'%s': Can't parse '%s' as state!", this->entity_id_, value.c_str());
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "color_mode", [this](StringRef value) {
    if (auto mode = parse_color_mode(value)) {
      this->import_(this->pending_.color_mode, *mode);
    } else if (value != "None") {
      ESP_LOGE(TAG, "'%s': Can't parse 'color_mode' value '%s' as color mode!", this->entity_id_, value.c_str());
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "brightness", [this](StringRef value) {
    if (auto brightness = this->parse_attribute_("brightness", value)) {
      this->import_(this->pending_.brightness, *brightness / 255.0f);
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "color_temp", [this](StringRef value) {
    if (auto mireds = this->parse_attribute_("color_temp", value)) {
      this->import_(this->pending_.color_temp, *mireds);
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "rgb_color", [this](StringRef value) {
    std::array<uint8_t, 3> rgb;
    if (parse_tuple(value.c_str(), rgb)) {
      this->import_(this->pending_.rgb, rgb);
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

template<typename T> void HomeassistantLight::import_(optional<T> &field, T value) {
  field = value;
  // One update changes several attributes, each arriving as its own message; apply them as one call
  if (!this->apply_scheduled_) {
    this->apply_scheduled_ = true;
    this->defer([this]() { this->apply_pending_(); });
  }
}

void HomeassistantLight::apply_pending_() {
  Pending pending = this->pending_;
  this->pending_ = {};
  this->apply_scheduled_ = false;

  auto call = this->state_->make_call().set_save(false);
  call.set_state(pending.state);
  call.set_color_mode(pending.color_mode);
  call.set_brightness(pending.brightness);
  call.set_color_temperature(pending.color_temp);
  if (pending.rgb.has_value()) {
    const auto &rgb = *pending.rgb;
    call.set_rgb(rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f);
  }
  ESP_LOGD(TAG, "'%s': Applying the state reported by Home Assistant", this->entity_id_);
  call.perform();
  // write_state() sees these values next and knows not to send them back
  this->imported_ = this->state_->remote_values;
}

void HomeassistantLight::write_state(LightState *state) {
  const LightColorValues &values = state->current_values;
  if (values != state->remote_values) {
    return;  // a transition runs here; Home Assistant gets the target once it is reached
  }
  if (values == this->imported_) {
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
  static constexpr auto COLOR_TEMP_KEY = StringRef::from_lit("color_temp");
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
      // In white mode the brightness goes under "white", which also selects that mode
      auto &level = resp.data.emplace_back();
      level.key = color_mode == ColorMode::WHITE ? WHITE_KEY : BRIGHTNESS_KEY;
      level.value = StringRef(level_buf, buf_append_printf(level_buf, sizeof(level_buf), 0, "%u",
                                                           static_cast<unsigned>(values.get_brightness() * 255)));
    }
    if (color_mode == ColorMode::COLOR_TEMPERATURE || color_mode == ColorMode::COLD_WARM_WHITE) {
      auto &color_temp = resp.data.emplace_back();
      color_temp.key = COLOR_TEMP_KEY;
      color_temp.value =
          StringRef(color_temp_buf, buf_append_printf(color_temp_buf, sizeof(color_temp_buf), 0, "%u",
                                                      static_cast<unsigned>(values.get_color_temperature())));
    }
    if (color_mode == ColorMode::RGB) {
      // A list, which only a template renders as one
      resp.data_template.init(1);
      auto &color = resp.data_template.emplace_back();
      color.key = RGB_COLOR_KEY;
      color.value = StringRef(color_buf, buf_append_printf(color_buf, sizeof(color_buf), 0, "[%u,%u,%u]",
                                                           static_cast<unsigned>(values.get_red() * 255),
                                                           static_cast<unsigned>(values.get_green() * 255),
                                                           static_cast<unsigned>(values.get_blue() * 255)));
    }
  }
  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
