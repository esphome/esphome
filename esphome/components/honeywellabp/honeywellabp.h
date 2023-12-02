// for Honeywell ABP sensor
// adapting code from https://github.com/vwls/Honeywell_pressure_sensors
#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"

namespace esphome {
namespace honeywellabp {

class HONEYWELLABPSensor : public PollingComponent,
                           public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                 spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_200KHZ> {
 public:
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void setup() override;
  void update() override;
  float get_setup_priority() const override;
  void dump_config() override;
  void set_honeywellabp_min_pressure(float min_pressure);
  void set_honeywellabp_max_pressure(float max_pressure);

 protected:
  float honeywellabp_min_pressure_ = 0.0;
  float honeywellabp_max_pressure_ = 0.0;
  uint8_t buf_[4];             // buffer to hold sensor data
  uint8_t status_ = 0;         // byte to hold status information.
  int pressure_count_ = 0;     // hold raw pressure data (14 - bit, 0 - 16384)
  int temperature_count_ = 0;  // hold raw temperature data (11 - bit, 0 - 2048)
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  uint8_t readsensor_();
  uint8_t readstatus_();
  int rawpressure_();
  int rawtemperature_();
  float countstopressure_(int counts, float min_pressure, float max_pressure);
  float countstotemperatures_(int counts);
  float read_pressure_();
  float read_temperature_();
};

}  // namespace honeywellabp
}  // namespace esphome
