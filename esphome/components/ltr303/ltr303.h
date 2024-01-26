#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/optional.h"

#include "ltr-definitions.h"

namespace esphome {
namespace ltr303 {

enum DataAvail : uint8_t { NO_DATA, BAD_DATA, DATA_OK };

class LTR303Component : public PollingComponent, public i2c::I2CDevice {
 public:
  //
  // EspHome framework functions
  //
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  //
  // Configuration setters
  //
  void set_gain(AlsGain gain) { this->gain_ = gain; }
  void set_integration_time(IntegrationTime time) { this->integration_time_ = time; }
  void set_repeat_rate(MeasurementRepeatRate rate) { this->repeat_rate_ = rate; }
  void set_glass_attenuation_factor(float factor) { this->glass_attenuation_factor_ = factor; }
  void set_enable_automatic_mode(bool enable) { this->automatic_mode_enabled_ = enable; }
  void set_enable_proximity_mode(bool enable) { this->proximity_mode_enabled_ = enable; }

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

  //
  // Current measurements data
  //
  struct Readings {
    uint16_t ch0{0};
    uint16_t ch1{0};
    AlsGain actual_gain{AlsGain::GAIN_1};
    IntegrationTime integration_time{IntegrationTime::INTEGRATION_TIME_100MS};
    float lux{0.0f};
  } readings_;

  //
  // LTR sensor type. 303/329 - als, 553 - als + proximity
  enum class LtrType : uint8_t {
    LtrUnknown = 0,
    LtrAlsOnly,
    LtrAlsAndProximity,
    LtrProximityOnly
  } ltr_type_{LtrType::LtrAlsOnly};

  //
  // Device interaction and data manipulation
  //
  bool identify_device_type_();
  void configure_reset_and_activate_();
  void configure_integration_time_(IntegrationTime time);
  void configure_gain_(AlsGain gain);
  DataAvail is_data_ready_(Readings &data);
  void read_sensor_data_(Readings &data);
  bool are_adjustments_required_(Readings &data);
  void apply_lux_calculation_(Readings &data);
  void publish_data_part_1_(Readings &data);
  void publish_data_part_2_(Readings &data);

  void configure_ps_();
  uint16_t read_ps_data_();
  uint16_t last_ps_data_{0xffff};

  //
  // Component configuration
  //
  bool automatic_mode_enabled_{true};
  bool proximity_mode_enabled_{false};
  AlsGain gain_{AlsGain::GAIN_1};
  IntegrationTime integration_time_{IntegrationTime::INTEGRATION_TIME_100MS};
  MeasurementRepeatRate repeat_rate_{MeasurementRepeatRate::REPEAT_RATE_500MS};
  float glass_attenuation_factor_{1.0};

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
};

}  // namespace ltr303
}  // namespace esphome
