#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

#include "esphome/components/sensor/sensor.h"

#include "as734xbase.h"

namespace esphome::as734x {

class AS734XComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup_model(Model model);

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_gain(Gain gain) { this->gain_ = gain; }
  void set_atime(uint8_t atime) { this->atime_ = atime; }
  void set_astep(uint16_t astep) { this->astep_ = astep; }

#ifdef USE_SENSOR
 protected:
  SensorArray band_counts_sensors_{};

 public:
  void set_counts_sensor(sensor::Sensor *sensor, uint8_t channel) {
    if (channel < this->band_counts_sensors_.size()) {
      this->band_counts_sensors_[channel] = sensor;
    }
  }
#endif

 protected:
  Model model_{Model::AS7343};
  AS734xBase *device_{nullptr};

  //
  // Internal state machine, used to split all the actions into
  // small steps in loop() to make sure we are not blocking execution
  //
  enum class State : uint8_t {
    NOT_INITIALIZED,
    IDLE,
    START_MEASUREMENT,
    CONFIGURE_SMUX,
    WAIT_SMUX,
    READ_DATA,
    DATA_COLLECTED,
    READY_TO_PUBLISH,
  } state_{State::NOT_INITIALIZED};

  uint16_t astep_;
  Gain gain_;
  uint8_t atime_;

  struct {
    ChannelValuesUint16 raw_counts{};
    Gain gain;
    uint32_t millis_start;
    uint8_t smux_step;
    bool first_run{true};
    bool valid{false};
  } readings_;

  void publish_channel_readings_();

#ifdef USE_SENSOR
  void publish_sensor_(sensor::Sensor *sensor, float value);
#endif
};

}  // namespace esphome::as734x
