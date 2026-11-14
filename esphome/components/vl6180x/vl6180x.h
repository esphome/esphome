#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/gpio.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome::vl6180x {

#ifndef VL6180X_INSTANCE_COUNT
// Code generation supplies this macro for firmware builds. Static analysis and
// host unit tests use a representative capacity when component codegen is off.
static constexpr uint8_t VL6180X_INSTANCE_COUNT = 2;
#endif

namespace testing {
class VL6180XComponentTest;
}

// ---------------------------------------------------------------------------
// Measurement state machine
//
// update() starts a range cycle:   IDLE → RANGE_START → RANGE_WAIT → IDLE
// lux_start_() starts an ALS cycle: IDLE → ALS_START  → ALS_WAIT  → IDLE
//
// The two are fully independent. lux_start_() is called by a set_interval
// timer at either the configured lux update_interval or, if none is set, at
// the same rate as the range update_interval — but always as a separate cycle.
//
// If lux_start_() fires while a range cycle is in progress, lux_pending_ is
// set and RANGE_WAIT chains to ALS_START after publishing the range result.
//
// When consecutive_failures_ reaches failure_threshold_, the sensor enters
// RECOVERY_WAIT. The instance at all_instances()[0] is the designated reinit
// initiator; it waits for all active measurements to finish, then calls
// run_init_sequence() which performs a full Option-1 reinit of every sensor.
// Non-initiator instances in RECOVERY_WAIT simply wait for the initiator to
// complete, then reset themselves to IDLE.
//
// loop() advances the active state on every main-loop iteration. Each state
// does minimal I2C work and returns immediately — no blocking of the main loop.
// ---------------------------------------------------------------------------
enum class MeasurementState : uint8_t {
  MEASUREMENT_STATE_IDLE,
  MEASUREMENT_STATE_RANGE_START,
  MEASUREMENT_STATE_RANGE_WAIT,
  MEASUREMENT_STATE_ALS_START,
  MEASUREMENT_STATE_ALS_WAIT,
  MEASUREMENT_STATE_RECOVERY_WAIT,
};

// Returns true if state represents an active measurement (not idle/recovery)
inline bool is_measuring(MeasurementState s) {
  return s == MeasurementState::MEASUREMENT_STATE_RANGE_START || s == MeasurementState::MEASUREMENT_STATE_RANGE_WAIT ||
         s == MeasurementState::MEASUREMENT_STATE_ALS_START || s == MeasurementState::MEASUREMENT_STATE_ALS_WAIT;
}

inline bool deadline_reached(uint32_t now, uint32_t deadline) { return static_cast<int32_t>(now - deadline) >= 0; }

class VL6180XComponent final : public PollingComponent, public i2c::I2CDevice {
 public:
  // Configuration setters — called from generated code
  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_range_sensor(sensor::Sensor *s) { this->range_sensor_ = s; }
  void set_scaling(uint8_t scaling) { this->scaling_ = scaling; }
  void set_als_gain(uint8_t gain, float factor) {
    this->als_gain_ = gain;
    this->als_gain_factor_ = factor;
  }
  void set_range_status_sensor(text_sensor::TextSensor *s) { this->range_status_sensor_ = s; }
  void set_lux_sensor(sensor::Sensor *s) { this->lux_sensor_ = s; }
  void set_lux_update_interval(uint32_t ms) { this->lux_update_interval_ms_ = ms; }
  void set_name_str(const char *name) { this->name_str_ = name; }

  // Recovery configuration — all optional, defaults applied in header
  void set_failure_threshold(uint8_t n) { this->failure_threshold_ = n; }
  void set_retry_interval(uint32_t ms) { this->retry_interval_ms_ = ms; }
  void set_max_retries(uint8_t n) { this->max_retries_ = n; }

  static void register_instance(VL6180XComponent *inst) { all_instances().push_back(inst); }

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

 protected:
  friend class testing::VL6180XComponentTest;
  uint32_t now_() const { return App.get_loop_component_start_time(); }

  // Low-level I2C
  bool write_reg8_(uint16_t reg, uint8_t value);
  bool write_reg16_(uint16_t reg, uint16_t value);
  bool read_reg8_(uint16_t reg, uint8_t &value);
  bool read_reg16_(uint16_t reg, uint16_t &value);

  // Initialisation
  bool assign_address_(uint8_t new_addr);
  bool init_sensor_();
  bool load_mandatory_settings_();
  bool configure_defaults_();
  bool apply_scaling_();

  // Full Option-1 reinit of all sensors — shared by setup() and reinit path.
  // Resets all instances to IDLE, runs the full XSHUT sequence, restores state.
  static void run_init_sequence();

  // State machine steps
  void do_range_start_();
  void do_range_wait_();
  void do_als_start_();
  void do_als_wait_();
  void do_recovery_wait_();

  // Kick off an ALS measurement; queues via lux_pending_ if busy
  void lux_start_();

  // Failure tracking — called at every error exit in measurement states
  void record_failure_();
  // Called on any successful publish — resets the consecutive failure counter
  void record_success_();
  void fail_range_measurement_(const char *status);
  void fail_lux_measurement_();

  // Publish helpers
  void publish_range_(uint8_t raw, uint8_t error_code);
  void publish_range_error_(const char *status);
  void publish_lux_(uint16_t raw);

  // ── Sensor configuration ─────────────────────────────────────────────────
  GPIOPin *enable_pin_{nullptr};
  InternalGPIOPin *interrupt_pin_{nullptr};
  sensor::Sensor *range_sensor_{nullptr};
  text_sensor::TextSensor *range_status_sensor_{nullptr};
  sensor::Sensor *lux_sensor_{nullptr};
  uint32_t lux_update_interval_ms_{0};
  const char *name_str_{"vl6180x"};
  uint8_t scaling_{1};
  uint8_t als_gain_{6};
  float als_gain_factor_{1.0f};
  int8_t ptp_offset_{0};

  // ── Operational state ────────────────────────────────────────────────────
  bool initialized_{false};
  MeasurementState state_{MeasurementState::MEASUREMENT_STATE_IDLE};
  uint32_t state_deadline_{0};
  bool lux_pending_{false};    // lux timer fired while range was in progress
  bool range_pending_{false};  // range timer fired while lux was in progress

  // ── Health monitoring ────────────────────────────────────────────────────
  uint8_t failure_threshold_{3};       // consecutive failures before recovery
  uint32_t retry_interval_ms_{30000};  // ms between reinit attempts
  uint8_t max_retries_{10};            // give up after this many failed reinits
  uint8_t consecutive_failures_{0};    // reset on any successful measurement
  uint8_t reinit_attempts_{0};         // attempts made in current recovery episode
  uint32_t reinit_due_at_{0};

  // ── Static instance registry ─────────────────────────────────────────────
  static SlotList<VL6180XComponent *, VL6180X_INSTANCE_COUNT> &all_instances() {
    static SlotList<VL6180XComponent *, VL6180X_INSTANCE_COUNT> instances;
    return instances;
  }
};

}  // namespace esphome::vl6180x
