#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::ens160_base {

class ENS160Component : public PollingComponent, public sensor::Sensor {
  SUB_SENSOR(co2)
  SUB_SENSOR(tvoc)
  SUB_SENSOR(aqi)

 public:
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }

  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  void send_env_data_();

  enum ValidityFlag {
    NORMAL_OPERATION = 0,
    WARMING_UP,
    INITIAL_STARTUP,
    INVALID_OUTPUT,
  } validity_flag_;

  bool warming_up_{false};
  bool initial_startup_{false};

  virtual bool read_byte(uint8_t a_register, uint8_t *data) = 0;
  virtual bool write_byte(uint8_t a_register, uint8_t data) = 0;
  virtual bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) = 0;
  virtual bool write_bytes(uint8_t a_register, uint8_t *data, size_t len) = 0;

  uint8_t firmware_ver_major_{0};
  uint8_t firmware_ver_minor_{0};
  uint8_t firmware_ver_build_{0};

  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *temperature_{nullptr};
};

}  // namespace esphome::ens160_base
