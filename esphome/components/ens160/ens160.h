#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ens160 {

class ENS160Component : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
 public:
  void set_co2(sensor::Sensor *co2) { co2_ = co2; }
  void set_tvoc(sensor::Sensor *tvoc) { tvoc_ = tvoc; }
  void set_aqi(sensor::Sensor *aqi) { aqi_ = aqi; }
  
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void send_env_data_();
  optional<uint8_t> read_status_();
  bool status_has_error_();
  bool status_has_data_();
  void reset_();

  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    INVALID_ID,
    VALIDITY_INVALID_OUTPUT,
    STANDARD_OPMODE_FAILED,
  } error_code_{NONE};
  
  enum ValidityFlag {
    NORMAL_OPERATION = 0,
    WARMING_UP,
    INITIAL_STARTUP,
    INVALID_OUTPUT,
  } validity_flag_;
  
  char firmware_version_[32];
  
  sensor::Sensor *co2_{nullptr};
  sensor::Sensor *tvoc_{nullptr};
  sensor::Sensor *aqi_{nullptr};
  
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *temperature_{nullptr};
};

}  // namespace ens160
}  // namespace esphome
