#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/finite_set_mask.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome::water_heater {

class WaterHeater;
struct WaterHeaterCallInternal;

void log_water_heater(const char *tag, const char *prefix, const char *type, WaterHeater *obj);
#define LOG_WATER_HEATER(prefix, type, obj) log_water_heater(TAG, prefix, LOG_STR_LITERAL(type), obj)

void call_water_heater_update(WaterHeater *a);
void register_water_heater(WaterHeater *a);

enum WaterHeaterMode : uint32_t {
  WATER_HEATER_MODE_OFF = 0,
  WATER_HEATER_MODE_ECO = 1,
  WATER_HEATER_MODE_ELECTRIC = 2,
  WATER_HEATER_MODE_PERFORMANCE = 3,
  WATER_HEATER_MODE_HIGH_DEMAND = 4,
  WATER_HEATER_MODE_HEAT_PUMP = 5,
  WATER_HEATER_MODE_GAS = 6,
};

// Type alias for water heater mode bitmask
// Replaces std::set<WaterHeaterMode> to eliminate red-black tree overhead
using WaterHeaterModeMask =
    FiniteSetMask<WaterHeaterMode, DefaultBitPolicy<WaterHeaterMode, WATER_HEATER_MODE_GAS + 1>>;

struct SavedWaterHeaterState {
  WaterHeaterMode mode;
  float target_temperature;
} __attribute__((packed));

class WaterHeaterCall {
  friend struct WaterHeaterCallInternal;

 public:
  WaterHeaterCall() : parent_(nullptr) {}

  WaterHeaterCall(WaterHeater *parent);

  WaterHeaterCall &set_mode(WaterHeaterMode mode);
  WaterHeaterCall &set_mode(const std::string &mode);
  WaterHeaterCall &set_target_temperature(float temperature);

  void perform();

  void apply(WaterHeater *water_heater);
  WaterHeaterCall &to_call(WaterHeater *water_heater);

  const optional<WaterHeaterMode> &get_mode() const { return this->mode_; }
  float get_target_temperature() const { return this->target_temperature_; }

 protected:
  void validate_();
  WaterHeater *parent_;
  optional<WaterHeaterMode> mode_;
  float target_temperature_{NAN};
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

  void set_supported_modes(WaterHeaterModeMask modes) { this->supported_modes_ = modes; }
  const WaterHeaterModeMask &get_supported_modes() const { return this->supported_modes_; }
  bool supports_mode(WaterHeaterMode mode) const { return this->supported_modes_.count(mode); }

 protected:
  // Ordered to minimize padding: 4-byte members first, then 1-byte bool
  float min_temperature_{0.0f};
  float max_temperature_{0.0f};
  WaterHeaterModeMask supported_modes_;
  bool supports_current_temperature_{false};
};

class WaterHeater : public EntityBase, public Component {
 public:
  WaterHeaterMode get_mode() const { return this->mode_; }
  float get_current_temperature() const { return this->current_temperature_; }
  float get_target_temperature() const { return this->target_temperature_; }

  void set_current_temperature(float current_temperature) { this->current_temperature_ = current_temperature; }

  virtual void publish_state();
  virtual WaterHeaterTraits get_traits();
  virtual WaterHeaterCallInternal make_call() = 0;

#ifdef USE_WATER_HEATER_VISUAL_OVERRIDES
  void set_visual_min_temperature_override(float min_temperature_override);
  void set_visual_max_temperature_override(float max_temperature_override);
#endif
  virtual void control(const WaterHeaterCall &call) = 0;

  void setup() override;

  optional<WaterHeaterCall> restore_state();

 protected:
  virtual WaterHeaterTraits traits() = 0;

  /// Set the mode of the water heater. Should only be called from control().
  void set_mode_(WaterHeaterMode mode) { this->mode_ = mode; }
  /// Set the target temperature of the water heater. Should only be called from control().
  void set_target_temperature_(float target_temperature) { this->target_temperature_ = target_temperature; }

  WaterHeaterMode mode_{WATER_HEATER_MODE_OFF};
  float current_temperature_{NAN};
  float target_temperature_{NAN};

#ifdef USE_WATER_HEATER_VISUAL_OVERRIDES
  float visual_min_temperature_override_{NAN};
  float visual_max_temperature_override_{NAN};
#endif

  uint32_t restore_storage_key_;
  ESPPreferenceObject pref_;
};

/// Convert the given WaterHeaterMode to a human-readable string for logging.
const LogString *water_heater_mode_to_string(WaterHeaterMode mode);

}  // namespace esphome::water_heater
