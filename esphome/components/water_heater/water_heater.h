#pragma once

#include <set>
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {

namespace water_heater {

struct WaterHeaterCallInternal;

class WaterHeater;
void call_water_heater_update(WaterHeater *a);
void register_water_heater(WaterHeater *a);

#define LOG_WATER_HEATER(tag, message, ...) ESP_LOGCONFIG(tag, message, __VA_ARGS__)

enum WaterHeaterMode : uint32_t {
  WATER_HEATER_MODE_OFF = 0,
  WATER_HEATER_MODE_ECO = 1,
  WATER_HEATER_MODE_ELECTRIC = 2,
  WATER_HEATER_MODE_PERFORMANCE = 3,
  WATER_HEATER_MODE_HIGH_DEMAND = 4,
  WATER_HEATER_MODE_HEAT_PUMP = 5,
  WATER_HEATER_MODE_GAS = 6,
};

struct SavedWaterHeaterState {
  WaterHeaterMode mode;
  float target_temperature;
} __attribute__((packed));

class WaterHeaterCall {
 public:
  WaterHeaterCall() : parent_(nullptr) {}

  WaterHeaterCall(WaterHeater *parent);

  WaterHeaterCall &set_mode(WaterHeaterMode mode);
  WaterHeaterCall &set_mode(const std::string &mode);
  WaterHeaterCall &set_target_temperature(float temperature);

  void perform();

  void apply(WaterHeater *water_heater);
  WaterHeaterCall &to_call(WaterHeater *water_heater);

  optional<WaterHeaterMode> mode_;
  optional<float> target_temperature_;

  const optional<WaterHeaterMode> &get_mode() const { return this->mode_; }
  const optional<float> &get_target_temperature() const { return this->target_temperature_; }

 protected:
  void validate_();
  WaterHeater *parent_;
};

struct WaterHeaterCallInternal : public WaterHeaterCall {
  WaterHeaterCallInternal(WaterHeater *parent) : WaterHeaterCall(parent) {}

  WaterHeaterCallInternal &set_from_restore(const WaterHeaterCall &restore) {
    this->mode_ = restore.mode_;
    this->target_temperature_ = restore.target_temperature_;
    return *this;
  }
};

class WaterHeaterTraits {
 public:
  void set_supports_current_temperature(bool supports_current_temperature);
  bool get_supports_current_temperature() const;

  void set_min_temperature(float min_temperature);
  float get_min_temperature() const;

  void set_max_temperature(float max_temperature);
  float get_max_temperature() const;

  void set_supported_modes(std::set<WaterHeaterMode> modes);
  const std::set<WaterHeaterMode> &get_supported_modes() const;
  bool supports_mode(WaterHeaterMode mode) const;

 protected:
  bool supports_current_temperature_{false};
  float min_temperature_{0.0f};
  float max_temperature_{0.0f};
  std::set<WaterHeaterMode> supported_modes_;
};

class WaterHeater : public EntityBase, public Component {
 public:
  WaterHeaterMode mode{WATER_HEATER_MODE_OFF};
  float current_temperature{NAN};
  float target_temperature{0.0f};

  virtual void publish_state();
  virtual WaterHeaterTraits get_traits();
  virtual WaterHeaterCallInternal make_call() = 0;

  virtual void set_visual_min_temperature_override(float min_temperature_override);
  virtual void set_visual_max_temperature_override(float max_temperature_override);
  virtual void control(const WaterHeaterCall &call) = 0;

  void setup() override;

  optional<WaterHeaterCall> restore_state();

 protected:
  virtual WaterHeaterTraits traits() = 0;

  optional<float> visual_min_temperature_override_{};
  optional<float> visual_max_temperature_override_{};

  uint32_t restore_storage_key_;
  ESPPreferenceObject pref_;
};

}  // namespace water_heater
}  // namespace esphome
