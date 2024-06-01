#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/optional.h"
#include "esphome/core/automation.h"

#include "ltr_definitions.h"

namespace esphome {
namespace ltr_als_ps {

enum DataAvail : uint8_t { NO_DATA, BAD_DATA, DATA_OK };

enum LtrType : uint8_t {
  LTR_TYPE_UNKNOWN = 0,
  LTR_TYPE_ALS_ONLY = 1,
  LTR_TYPE_PS_ONLY = 2,
  LTR_TYPE_ALS_AND_PS = 3,
};

class LTRAlsPsComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  //
  // EspHome framework functions
  //
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  // Configuration setters : General
  //
  void set_ltr_type(LtrType type) { this->ltr_type_ = type; }

  // Configuration setters : ALS
  //
  void set_als_auto_mode(bool enable) { this->automatic_mode_enabled_ = enable; }
  void set_als_gain(AlsGain gain) { this->gain_ = gain; }
  void set_als_integration_time(IntegrationTime time) { this->integration_time_ = time; }
  void set_als_meas_repeat_rate(MeasurementRepeatRate rate) { this->repeat_rate_ = rate; }
  void set_als_glass_attenuation_factor(float factor) { this->glass_attenuation_factor_ = factor; }

  // Configuration setters : PS
  //
  void set_ps_high_threshold(uint16_t threshold) { this->ps_threshold_high_ = threshold; }
  void set_ps_low_threshold(uint16_t threshold) { this->ps_threshold_low_ = threshold; }
  void set_ps_cooldown_time_s(uint16_t time) { this->ps_cooldown_time_s_ = time; }
  void set_ps_gain(PsGain gain) { this->ps_gain_ = gain; }

  // Sensors setters
  //
  void set_ambient_light_sensor(sensor::Sensor *sensor) { this->ambient_light_sensor_ = sensor; }
  void set_full_spectrum_counts_sensor(sensor::Sensor *sensor) { this->full_spectrum_counts_sensor_ = sensor; }
  void set_infrared_counts_sensor(sensor::Sensor *sensor) { this->infrared_counts_sensor_ = sensor; }
  void set_actual_gain_sensor(sensor::Sensor *sensor) { this->actual_gain_sensor_ = sensor; }
  void set_actual_integration_time_sensor(sensor::Sensor *sensor) { this->actual_integration_time_sensor_ = sensor; }
  void set_proximity_counts_sensor(sensor::Sensor *sensor) { this->proximity_counts_sensor_ = sensor; }

 protected:
  //
  // Internal state machine, used to split all the actions into
  // small steps in loop() to make sure we are not blocking execution
  //
  enum class State : uint8_t {
    NOT_INITIALIZED,
    DELAYED_SETUP,
    IDLE,
    WAITING_FOR_DATA,
    COLLECTING_DATA_AUTO,
    DATA_COLLECTED,
    ADJUSTMENT_IN_PROGRESS,
    READY_TO_PUBLISH,
    KEEP_PUBLISHING
  } state_{State::NOT_INITIALIZED};

  LtrType ltr_type_{LtrType::LTR_TYPE_ALS_ONLY};

  //
  // Current measurements data
  //
  struct AlsReadings {
    uint16_t ch0{0};
    uint16_t ch1{0};
    AlsGain gain{AlsGain::GAIN_1};
    IntegrationTime integration_time{IntegrationTime::INTEGRATION_TIME_100MS};
    float lux{0.0f};
    uint8_t number_of_adjustments{0};
  } als_readings_;
  uint16_t ps_readings_{0xfffe};

  inline bool is_als_() const {
    return this->ltr_type_ == LtrType::LTR_TYPE_ALS_ONLY || this->ltr_type_ == LtrType::LTR_TYPE_ALS_AND_PS;
  }
  inline bool is_ps_() const {
    return this->ltr_type_ == LtrType::LTR_TYPE_PS_ONLY || this->ltr_type_ == LtrType::LTR_TYPE_ALS_AND_PS;
  }

  //
  // Device interaction and data manipulation
  //
  bool check_part_number_();

  void configure_reset_();
  void configure_als_();
  void configure_integration_time_(IntegrationTime time);
  void configure_gain_(AlsGain gain);
  DataAvail is_als_data_ready_(AlsReadings &data);
  void read_sensor_data_(AlsReadings &data);
  bool are_adjustments_required_(AlsReadings &data);
  void apply_lux_calculation_(AlsReadings &data);
  void publish_data_part_1_(AlsReadings &data);
  void publish_data_part_2_(AlsReadings &data);

  void configure_ps_();
  uint16_t read_ps_data_();
  void check_and_trigger_ps_();

  //
  // Component configuration
  //
  bool automatic_mode_enabled_{true};
  AlsGain gain_{AlsGain::GAIN_1};
  IntegrationTime integration_time_{IntegrationTime::INTEGRATION_TIME_100MS};
  MeasurementRepeatRate repeat_rate_{MeasurementRepeatRate::REPEAT_RATE_500MS};
  float glass_attenuation_factor_{1.0};

  uint16_t ps_cooldown_time_s_{5};
  PsGain ps_gain_{PsGain::PS_GAIN_16};
  uint16_t ps_threshold_high_{0xffff};
  uint16_t ps_threshold_low_{0x0000};

  //
  //   Sensors for publishing data
  //
  sensor::Sensor *infrared_counts_sensor_{nullptr};          // direct reading CH1, infrared only
  sensor::Sensor *full_spectrum_counts_sensor_{nullptr};     // direct reading CH0, infrared + visible light
  sensor::Sensor *ambient_light_sensor_{nullptr};            // calculated lux
  sensor::Sensor *actual_gain_sensor_{nullptr};              // actual gain of reading
  sensor::Sensor *actual_integration_time_sensor_{nullptr};  // actual integration time
  sensor::Sensor *proximity_counts_sensor_{nullptr};         // proximity sensor

  bool is_any_als_sensor_enabled_() const {
    return this->ambient_light_sensor_ != nullptr || this->full_spectrum_counts_sensor_ != nullptr ||
           this->infrared_counts_sensor_ != nullptr || this->actual_gain_sensor_ != nullptr ||
           this->actual_integration_time_sensor_ != nullptr;
  }
  bool is_any_ps_sensor_enabled_() const { return this->proximity_counts_sensor_ != nullptr; }

  //
  // Trigger section for the automations
  //
  friend class LTRPsHighTrigger;
  friend class LTRPsLowTrigger;

  CallbackManager<void()> on_ps_high_trigger_callback_;
  CallbackManager<void()> on_ps_low_trigger_callback_;

  void add_on_ps_high_trigger_callback_(std::function<void()> callback) {
    this->on_ps_high_trigger_callback_.add(std::move(callback));
  }

  void add_on_ps_low_trigger_callback_(std::function<void()> callback) {
    this->on_ps_low_trigger_callback_.add(std::move(callback));
  }
};

class LTRPsHighTrigger : public Trigger<> {
 public:
  explicit LTRPsHighTrigger(LTRAlsPsComponent *parent) {
    parent->add_on_ps_high_trigger_callback_([this]() { this->trigger(); });
  }
};

class LTRPsLowTrigger : public Trigger<> {
 public:
  explicit LTRPsLowTrigger(LTRAlsPsComponent *parent) {
    parent->add_on_ps_low_trigger_callback_([this]() { this->trigger(); });
  }
};
}  // namespace ltr_als_ps
}  // namespace esphome
