#include "esphome/core/defines.h"
#ifdef USE_ESP32
#include "ecocomfort2_fan.h"
#include "esphome/core/log.h"

#include <iterator>

namespace esphome::ecocomfort2 {

ESPHOME_LOG_TAG(TAG, "ecocomfort2.fan");

static constexpr int SPEED_COUNT = SPEED_VEL3;

struct Preset {
  const char *name;
  uint8_t mode;
  bool auto_mode;
};

// Order matters: the fan core persists the preset as an index into this list.
static constexpr Preset PRESETS[] = {
    {"In", OPER_IN, false},
    {"Out", OPER_OUT, false},
    {"In-Out", OPER_IN_OUT, false},
    {"Sensor", OPER_SENSOR_OR_AUTO, false},
    {"Auto", OPER_SENSOR_OR_AUTO, true},
};
static constexpr const Preset &PRESET_IN_OUT = PRESETS[2];
static constexpr const Preset &PRESET_SENSOR = PRESETS[3];

static const Preset *find_preset(const char *name) {
  for (const auto &preset : PRESETS) {
    if (preset.name == name)
      return &preset;
  }
  return nullptr;
}

static const Preset *find_preset(uint8_t mode, bool auto_mode) {
  for (const auto &preset : PRESETS) {
    if (preset.mode == mode && preset.auto_mode == auto_mode)
      return &preset;
  }
  return nullptr;
}

void Ecocomfort2Fan::setup() {
  this->set_supported_preset_modes(
      {PRESETS[0].name, PRESETS[1].name, PRESETS[2].name, PRESETS[3].name, PRESETS[4].name});
  // Restore the last preset and speed so turning on resumes them; on/off comes from the device.
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    this->speed = restore->speed;
    if (restore->preset_mode < std::size(PRESETS))
      this->set_preset_mode_(PRESETS[restore->preset_mode].name);
  }
}

void Ecocomfort2Fan::dump_config() { LOG_FAN("", "Ecocomfort2 Fan", this); }

fan::FanTraits Ecocomfort2Fan::get_traits() {
  fan::FanTraits traits;
  traits.set_speed(true);
  traits.set_supported_speed_count(SPEED_COUNT);
  this->wire_preset_modes_(traits);
  return traits;
}

void Ecocomfort2Fan::control(const fan::FanCall &call) {
  if (!this->parent_->is_ready()) {
    ESP_LOGW(TAG, "Not ready, cannot handle control call");
    return;
  }

  // Any call that does not turn the fan off turns it on
  auto state = call.get_state();
  this->state = !state.has_value() || *state;
  // The core clears the preset in a speed call; here the preset is the air flow mode, so keep it.
  if (call.has_preset_mode())
    this->set_preset_mode_(call.get_preset_mode());

  const Preset *preset = find_preset(this->get_preset_mode().c_str());
  if (preset == nullptr) {
    preset = &PRESET_IN_OUT;
    this->set_preset_mode_(preset->name);
  }
  auto speed = call.get_speed();
  if (speed.has_value()) {
    this->speed = *speed;
    // A manual speed leaves automatic speed control
    if (preset->auto_mode) {
      preset = &PRESET_SENSOR;
      this->set_preset_mode_(preset->name);
    }
  }
  this->speed = clamp(this->speed, 1, SPEED_COUNT);

  if (!this->state) {
    this->parent_->write_oper(OPER_OFF, SPEED_OFF);
  } else {
    this->parent_->write_oper(preset->mode, preset->auto_mode ? FLAG_AUTO : static_cast<uint8_t>(this->speed));
  }
  this->publish_state();
}

void Ecocomfort2Fan::on_status() {
  uint8_t mode = this->parent_->get_mode();
  uint8_t raw_speed = this->parent_->get_speed();

  // Off keeps the preset and speed so turning on resumes them
  if (mode == OPER_OFF) {
    if (!this->state)
      return;
    this->state = false;
    this->publish_state();
    return;
  }

  int new_speed = this->speed;
  uint8_t base_speed = raw_speed & SPEED_MASK;
  if (base_speed >= SPEED_SLEEP && base_speed <= SPEED_VEL3)
    new_speed = base_speed;
  const Preset *preset = find_preset(mode, (raw_speed & FLAG_AUTO) != 0);
  const char *new_preset = preset != nullptr ? preset->name : nullptr;
  bool same_preset = new_preset == nullptr ? !this->has_preset_mode() : this->get_preset_mode().c_str() == new_preset;
  if (this->state && this->speed == new_speed && same_preset)
    return;

  this->state = true;
  this->speed = new_speed;
  if (new_preset != nullptr) {
    this->set_preset_mode_(new_preset);
  } else {
    this->clear_preset_mode_();
  }
  this->publish_state();
}

}  // namespace esphome::ecocomfort2

#endif  // USE_ESP32
