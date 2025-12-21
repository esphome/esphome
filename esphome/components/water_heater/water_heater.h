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

/// Feature flags for water heater capabilities (matches Home Assistant WaterHeaterEntityFeature)
enum WaterHeaterFeature : uint32_t {
  /// The water heater supports reporting the current temperature.
  WATER_HEATER_SUPPORTS_CURRENT_TEMPERATURE = 1 << 0,
  /// The water heater supports a target temperature.
  WATER_HEATER_SUPPORTS_TARGET_TEMPERATURE = 1 << 1,
  /// The water heater supports operation mode selection.
  WATER_HEATER_SUPPORTS_OPERATION_MODE = 1 << 2,
  /// The water heater supports an away/vacation mode.
  WATER_HEATER_SUPPORTS_AWAY_MODE = 1 << 3,
  /// The water heater can be turned on/off.
  WATER_HEATER_SUPPORTS_ON_OFF = 1 << 4,
  /// The water heater supports two-point target temperature (low/high range).
  WATER_HEATER_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE = 1 << 5,
};

/// State flags for water heater current state (bitmask)
enum WaterHeaterStateFlag : uint32_t {
  /// Away/vacation mode is currently active
  WATER_HEATER_STATE_AWAY = 1 << 0,
  /// Water heater is on (not in standby)
  WATER_HEATER_STATE_ON = 1 << 1,
};

struct SavedWaterHeaterState {
  WaterHeaterMode mode;
  union {
    float target_temperature;
    struct {
      float target_temperature_low;
      float target_temperature_high;
    };
  } __attribute__((packed));
  uint32_t state;
} __attribute__((packed));

class WaterHeaterCall {
  friend struct WaterHeaterCallInternal;

 public:
  WaterHeaterCall() : parent_(nullptr) {}

  WaterHeaterCall(WaterHeater *parent);

  WaterHeaterCall &set_mode(WaterHeaterMode mode);
  WaterHeaterCall &set_mode(const std::string &mode);
  WaterHeaterCall &set_target_temperature(float temperature);
  WaterHeaterCall &set_target_temperature_low(float temperature);
  WaterHeaterCall &set_target_temperature_high(float temperature);
  WaterHeaterCall &set_away(bool away);
  WaterHeaterCall &set_on(bool on);

  void perform();

  const optional<WaterHeaterMode> &get_mode() const { return this->mode_; }
  float get_target_temperature() const { return this->target_temperature_; }
  float get_target_temperature_low() const { return this->target_temperature_low_; }
  float get_target_temperature_high() const { return this->target_temperature_high_; }
  /// Get state flags value
  uint32_t get_state() const { return this->state_; }

 protected:
  void validate_();
  WaterHeater *parent_;
  optional<WaterHeaterMode> mode_;
  float target_temperature_{NAN};
  float target_temperature_low_{NAN};
  float target_temperature_high_{NAN};
  uint32_t state_{0};
};

struct WaterHeaterCallInternal : public WaterHeaterCall {
  WaterHeaterCallInternal(WaterHeater *parent) : WaterHeaterCall(parent) {}

  WaterHeaterCallInternal &set_from_restore(const WaterHeaterCall &restore) {
    this->mode_ = restore.mode_;
    this->target_temperature_ = restore.target_temperature_;
    this->target_temperature_low_ = restore.target_temperature_low_;
    this->target_temperature_high_ = restore.target_temperature_high_;
    this->state_ = restore.state_;
    return *this;
  }
};

class WaterHeaterTraits {
 public:
  /// Get/set feature flags (see WaterHeaterFeature enum)
  void add_feature_flags(uint32_t flags) { this->feature_flags_ |= flags; }
  void clear_feature_flags(uint32_t flags) { this->feature_flags_ &= ~flags; }
  bool has_feature_flags(uint32_t flags) const { return (this->feature_flags_ & flags) == flags; }
  uint32_t get_feature_flags() const { return this->feature_flags_; }

  bool get_supports_current_temperature() const {
    return this->has_feature_flags(WATER_HEATER_SUPPORTS_CURRENT_TEMPERATURE);
  }
  void set_supports_current_temperature(bool supports) {
    if (supports) {
      this->add_feature_flags(WATER_HEATER_SUPPORTS_CURRENT_TEMPERATURE);
    } else {
      this->clear_feature_flags(WATER_HEATER_SUPPORTS_CURRENT_TEMPERATURE);
    }
  }

  bool get_supports_away_mode() const { return this->has_feature_flags(WATER_HEATER_SUPPORTS_AWAY_MODE); }
  void set_supports_away_mode(bool supports) {
    if (supports) {
      this->add_feature_flags(WATER_HEATER_SUPPORTS_AWAY_MODE);
    } else {
      this->clear_feature_flags(WATER_HEATER_SUPPORTS_AWAY_MODE);
    }
  }

  bool get_supports_two_point_target_temperature() const {
    return this->has_feature_flags(WATER_HEATER_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE);
  }
  void set_supports_two_point_target_temperature(bool supports) {
    if (supports) {
      this->add_feature_flags(WATER_HEATER_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE);
    } else {
      this->clear_feature_flags(WATER_HEATER_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE);
    }
  }

  void set_min_temperature(float min_temperature) { this->min_temperature_ = min_temperature; }
  float get_min_temperature() const { return this->min_temperature_; }

  void set_max_temperature(float max_temperature) { this->max_temperature_ = max_temperature; }
  float get_max_temperature() const { return this->max_temperature_; }

  void set_target_temperature_step(float target_temperature_step) {
    this->target_temperature_step_ = target_temperature_step;
  }
  float get_target_temperature_step() const { return this->target_temperature_step_; }

  void set_supported_modes(WaterHeaterModeMask modes) { this->supported_modes_ = modes; }
  const WaterHeaterModeMask &get_supported_modes() const { return this->supported_modes_; }
  bool supports_mode(WaterHeaterMode mode) const { return this->supported_modes_.count(mode); }

 protected:
  // Ordered to minimize padding: 4-byte members first
  uint32_t feature_flags_{0};
  float min_temperature_{0.0f};
  float max_temperature_{0.0f};
  float target_temperature_step_{0.0f};
  WaterHeaterModeMask supported_modes_;
};

class WaterHeater : public EntityBase, public Component {
 public:
  WaterHeaterMode get_mode() const { return this->mode_; }
  float get_current_temperature() const { return this->current_temperature_; }
  float get_target_temperature() const { return this->target_temperature_; }
  float get_target_temperature_low() const { return this->target_temperature_low_; }
  float get_target_temperature_high() const { return this->target_temperature_high_; }
  /// Get the current state flags bitmask
  uint32_t get_state() const { return this->state_; }
  /// Check if away mode is currently active
  bool is_away() const { return (this->state_ & WATER_HEATER_STATE_AWAY) != 0; }
  /// Check if the water heater is on
  bool is_on() const { return (this->state_ & WATER_HEATER_STATE_ON) != 0; }

  void set_current_temperature(float current_temperature) { this->current_temperature_ = current_temperature; }

  virtual void publish_state();
  virtual WaterHeaterTraits get_traits();
  virtual WaterHeaterCallInternal make_call() = 0;

#ifdef USE_WATER_HEATER_VISUAL_OVERRIDES
  void set_visual_min_temperature_override(float min_temperature_override);
  void set_visual_max_temperature_override(float max_temperature_override);
  void set_visual_target_temperature_step_override(float visual_target_temperature_step_override);
#endif
  virtual void control(const WaterHeaterCall &call) = 0;

  void setup() override;

  optional<WaterHeaterCall> restore_state();

 protected:
  virtual WaterHeaterTraits traits() = 0;

  /// Log the traits of this water heater for dump_config().
  void dump_traits_(const char *tag);

  /// Set the mode of the water heater. Should only be called from control().
  void set_mode_(WaterHeaterMode mode) { this->mode_ = mode; }
  /// Set the target temperature of the water heater. Should only be called from control().
  void set_target_temperature_(float target_temperature) { this->target_temperature_ = target_temperature; }
  /// Set the low target temperature (for two-point control). Should only be called from control().
  void set_target_temperature_low_(float target_temperature_low) {
    this->target_temperature_low_ = target_temperature_low;
  }
  /// Set the high target temperature (for two-point control). Should only be called from control().
  void set_target_temperature_high_(float target_temperature_high) {
    this->target_temperature_high_ = target_temperature_high;
  }
  /// Set the state flags. Should only be called from control().
  void set_state_(uint32_t state) { this->state_ = state; }
  /// Set or clear a state flag. Should only be called from control().
  void set_state_flag_(uint32_t flag, bool value) {
    if (value) {
      this->state_ |= flag;
    } else {
      this->state_ &= ~flag;
    }
  }

  WaterHeaterMode mode_{WATER_HEATER_MODE_OFF};
  float current_temperature_{NAN};
  float target_temperature_{NAN};
  float target_temperature_low_{NAN};
  float target_temperature_high_{NAN};
  uint32_t state_{0};  // Bitmask of WaterHeaterStateFlag

#ifdef USE_WATER_HEATER_VISUAL_OVERRIDES
  float visual_min_temperature_override_{NAN};
  float visual_max_temperature_override_{NAN};
  float visual_target_temperature_step_override_{NAN};
#endif

  ESPPreferenceObject pref_;
};

/// Convert the given WaterHeaterMode to a human-readable string for logging.
const LogString *water_heater_mode_to_string(WaterHeaterMode mode);

}  // namespace esphome::water_heater
