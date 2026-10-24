#include "homeassistant_light.h"

#include <algorithm>
#include <cmath>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "../homeassistant.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.light");

static constexpr uint32_t ECHO_TIMEOUT_ID = 0;

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

optional<ColorMode> parse_color_mode(StringRef name) {
  const int16_t i = progmem_string_table_find<ColorModeNames>(name.c_str(), name.size());
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
  // Home Assistant sends these on subscribe and when they change, so a light that comes up late gets them
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
    this->import_(this->imported_.state, optional<bool>(on_off == PARSE_ON));
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
    this->import_(this->imported_.color_mode, *mode);
  });
  server->subscribe_home_assistant_state(this->entity_id_, "brightness", [this](StringRef value) {
    if (auto brightness = this->parse_integer_("brightness", value, 255)) {
      this->import_(this->imported_.brightness, static_cast<uint8_t>(*brightness));
    }
  });
  server->subscribe_home_assistant_state(this->entity_id_, "color_temp_kelvin", [this](StringRef value) {
    if (auto kelvin = this->parse_integer_("color_temp_kelvin", value, UINT16_MAX)) {
      this->import_(this->imported_.color_temp_kelvin, *kelvin);
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
    this->on_report_(!this->imported_.has_rgb || this->imported_.rgb != rgb);
    this->imported_.rgb = rgb;
    this->imported_.has_rgb = true;
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
  const bool in_range = *number >= 1.0f && *number <= max;
  if (!in_range) {
    ESP_LOGW(TAG, "'%s': '%s' value %.0f is out of range!", this->entity_id_, name, *number);
    return nullopt;
  }
  return static_cast<uint16_t>(*number);
}

void HomeassistantLight::on_report_(bool changed) {
  // A report that changed an attribute answers the change that armed the echo timeout; a repeated
  // value (or a trait) does not, so a refused change still gets its warning
  if (changed) {
    if (this->echo_pending_) {
      this->echo_pending_ = false;
      this->cancel_timeout(ECHO_TIMEOUT_ID);
    }
    // Noted as the report arrives, not when the deferred apply runs: a call read from the same loop
    // pass before this report is older than it, one read after it is newer
    this->state_->clear_called();
  }
  this->schedule_apply_();
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
  // Nothing to apply before the first state; while a sent change waits for its report the reported
  // state is the old one, so a trait that arrives then does not bring it back
  if (!this->imported_.state.has_value() || this->echo_pending_) {
    return;
  }
  // Attributes Home Assistant has not reported keep what the light has. An "off" report carries
  // no levels, so the ones reported last stay with the light: a turn_on without a brightness
  // then sends what Home Assistant last had, not a level that never reached it
  const Attributes &imported = this->imported_;
  LightColorValues values = this->state_->remote_values;
  values.set_state(*imported.state);
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
  if (!this->state_->set_external_state(values)) {
    // A report that changed something during a transition or flash is applied once it ends, in
    // place of the values it ends at, unless a call came after the report; a trait or a repeated
    // report has nothing newer to say
    if (this->state_->is_transitioning() && !this->state_->called_since_clear()) {
      this->report_held_ = true;
      return;
    }
    // A local change still waits for LightState::loop() to send it. Home Assistant reports again
    // once that change reaches it; when it does not, the echo timeout write_state() arms brings the
    // light back to what was reported last
    ESP_LOGD(TAG, "'%s': Report dropped, a local change is on its way", this->entity_id_);
    return;
  }
  ESP_LOGV(TAG, "'%s': Took over the state reported by Home Assistant", this->entity_id_);
}

void HomeassistantLight::update_state(LightState *state) {
  // The light core's setup performs one call for the initial state; a second one before the first
  // write (an on_boot automation) is a local change merged into that write, so it must be sent
  if (this->initial_write_) {
    if (this->initial_seen_) {
      this->initial_write_ = false;
    }
    this->initial_seen_ = true;
  }
  // Every call that changes the light reaches here within a loop pass (a new transformer's first
  // apply() or set_immediately_()), and a call since the held report was read is newer than it
  if (this->report_held_ && state->called_since_clear()) {
    this->report_held_ = false;
  }
}

bool HomeassistantLight::is_imported_(const Attributes &outgoing) const {
  const Attributes &imported = this->imported_;
  if (!outgoing.state.has_value() || imported.state != outgoing.state) {
    return false;
  }
  if (!*outgoing.state) {
    return true;
  }
  if (imported.color_mode != outgoing.color_mode || imported.brightness != outgoing.brightness) {
    return false;
  }
  // rgb_color stays reported in the other modes, so only the attributes of the mode count
  switch (outgoing.color_mode) {
    case ColorMode::COLOR_TEMPERATURE:
      return imported.color_temp_kelvin == outgoing.color_temp_kelvin;
    case ColorMode::RGB:
      return imported.has_rgb && imported.rgb == outgoing.rgb;
    default:
      return true;
  }
}

void HomeassistantLight::fall_back_() {
  // Back to the state Home Assistant reported, which the light still has
  if (!this->imported_.state.has_value()) {
    ESP_LOGW(TAG, "'%s': Nothing reported yet to fall back to, the light keeps the unsent change", this->entity_id_);
    return;
  }
  this->schedule_apply_();
}

void HomeassistantLight::write_state(LightState *state) {
  // A reported state is taken over without a write, so every write is a local change, except
  // the very first: the light core writes its initial state, which Home Assistant does not need
  if (this->initial_write_) {
    this->initial_write_ = false;
    return;
  }
  if (state->is_transitioning()) {
    // A transition or flash runs here alone; Home Assistant gets the state it ends at
    this->transitioned_ = true;
    return;
  }
  const bool held = this->report_held_;
  const bool transitioned = this->transitioned_;
  this->report_held_ = false;
  this->transitioned_ = false;
  // Home Assistant changed meanwhile; its report is newer than what the transition ends at or
  // what the flash goes back to
  if (held) {
    ESP_LOGD(TAG, "'%s': Applying the report that arrived during the transition", this->entity_id_);
    this->schedule_apply_();
    return;
  }
  // What Home Assistant holds after the change. It has one level, so the channel brightness of the
  // mode folds into it
  const LightColorValues &values = state->current_values;
  Attributes outgoing;
  outgoing.state = values.is_on();
  if (values.is_on()) {
    outgoing.color_mode = values.get_color_mode();
    switch (outgoing.color_mode) {
      case ColorMode::ON_OFF:
        break;
      case ColorMode::WHITE:
        outgoing.brightness = to_uint8_scale(values.get_brightness() * values.get_white());
        break;
      case ColorMode::COLOR_TEMPERATURE:
        outgoing.brightness = to_uint8_scale(values.get_brightness());
        if (values.get_color_temperature() > 0.0f) {
          // A mired value below the reported range could give more than 65535 K
          outgoing.color_temp_kelvin = lroundf(std::min(values.get_color_temperature_kelvin(), 65535.0f));
        }
        break;
      case ColorMode::RGB:
        outgoing.brightness = to_uint8_scale(values.get_brightness() * values.get_color_brightness());
        outgoing.rgb = {to_uint8_scale(values.get_red()), to_uint8_scale(values.get_green()),
                        to_uint8_scale(values.get_blue())};
        outgoing.has_rgb = true;
        break;
      default:
        outgoing.brightness = to_uint8_scale(values.get_brightness());
        break;
    }
  }
  // Home Assistant reports only what changed, so a change that leaves it as it is gets no report:
  // nothing to send and no echo to wait for. A change still on its way may alter it first, though
  if (!this->echo_pending_ && this->is_imported_(outgoing)) {
    if (transitioned) {
      // A flash ends where it began, and Home Assistant only ever gets the end
      ESP_LOGW(TAG, "'%s': Ended at the state Home Assistant already has, nothing to send (a flash never is)",
               this->entity_id_);
    } else {
      ESP_LOGD(TAG, "'%s': Home Assistant already has this state", this->entity_id_);
    }
    return;
  }
  if (!api::global_api_server->is_connected()) {
    ESP_LOGE(TAG, "'%s': No clients connected to API server, change lost", this->entity_id_);
    this->fall_back_();
    return;
  }
#ifdef USE_API_WIZARD_LINKED_INPUTS
  if (this->entity_id_[0] == '\0') {
    ESP_LOGW(TAG, "'%s': No entity ID set yet", LOG_STR_ARG(state->get_log_name()));
    this->fall_back_();
    return;
  }
#endif

  // In flash on the ESP8266 and encoded from there
  static const char SERVICE_TURN_ON[] PROGMEM = "light.turn_on";
  static const char SERVICE_TURN_OFF[] PROGMEM = "light.turn_off";
  static const char ENTITY_ID_KEY[] PROGMEM = "entity_id";
  static const char BRIGHTNESS_KEY[] PROGMEM = "brightness";
  static const char WHITE_KEY[] PROGMEM = "white";
  static const char COLOR_TEMP_KELVIN_KEY[] PROGMEM = "color_temp_kelvin";
  static const char RGB_COLOR_KEY[] PROGMEM = "rgb_color";

  // The request refers to these buffers until it is sent below
  char level_buf[4];       // up to 255
  char color_temp_buf[6];  // up to 65535
  char color_buf[16];

  api::HomeassistantActionRequest resp;
  resp.data.init(3);
  auto &entity_id = resp.data.emplace_back();
  entity_id.key = ProgmemStringRef::from_lit(ENTITY_ID_KEY);
  entity_id.value = ProgmemStringRef(StringRef(this->entity_id_));

  auto add_number = [&](ProgmemStringRef key, char *buf, size_t size, unsigned number) {
    auto &entry = resp.data.emplace_back();
    entry.key = key;
    entry.value = ProgmemStringRef(StringRef(buf, buf_append_printf(buf, size, 0, "%u", number)));
  };
  if (!*outgoing.state) {
    resp.service = ProgmemStringRef::from_lit(SERVICE_TURN_OFF);
  } else {
    resp.service = ProgmemStringRef::from_lit(SERVICE_TURN_ON);
    if (outgoing.color_mode != ColorMode::ON_OFF) {
      // The level goes under "white" in that mode, which also selects the mode
      add_number(outgoing.color_mode == ColorMode::WHITE ? ProgmemStringRef::from_lit(WHITE_KEY)
                                                         : ProgmemStringRef::from_lit(BRIGHTNESS_KEY),
                 level_buf, sizeof(level_buf), outgoing.brightness);
    }
    // Home Assistant converts the color to the light's own channels, which loses the white balance
    // of a cold/warm white light, so the color goes only when it or the mode changed (or may have,
    // while a change still waits for its report)
    const Attributes &imported = this->imported_;
    const bool new_color = this->echo_pending_ || outgoing.color_mode != imported.color_mode;
    if (outgoing.color_temp_kelvin != 0 && (new_color || outgoing.color_temp_kelvin != imported.color_temp_kelvin)) {
      add_number(ProgmemStringRef::from_lit(COLOR_TEMP_KELVIN_KEY), color_temp_buf, sizeof(color_temp_buf),
                 outgoing.color_temp_kelvin);
    }
    if (outgoing.has_rgb && (new_color || !imported.has_rgb || outgoing.rgb != imported.rgb)) {
      // A list, which only a template renders as one
      resp.data_template.init(1);
      auto &color = resp.data_template.emplace_back();
      color.key = ProgmemStringRef::from_lit(RGB_COLOR_KEY);
      color.value =
          ProgmemStringRef(StringRef(color_buf, buf_append_printf(color_buf, sizeof(color_buf), 0, "[%u,%u,%u]",
                                                                  outgoing.rgb[0], outgoing.rgb[1], outgoing.rgb[2])));
    }
  }
  api::global_api_server->send_homeassistant_action(resp);
  // Home Assistant reports the change back in a moment, which cancels this; when it does not, the
  // change got lost on the way (no subscriber yet, the entity unavailable, the action refused)
  this->echo_pending_ = true;
  this->set_timeout(ECHO_TIMEOUT_ID, this->report_timeout_s_ * 1000u, [this]() {
    this->echo_pending_ = false;
    ESP_LOGW(TAG, "'%s': Home Assistant did not report the change back", this->entity_id_);
    this->fall_back_();
  });
}

}  // namespace esphome::homeassistant
