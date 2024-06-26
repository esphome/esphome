#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mcp9600 {

enum MCP9600ThermocoupleType : uint8_t {
  MCP9600_THERMOCOUPLE_TYPE_K = 0b000,
  MCP9600_THERMOCOUPLE_TYPE_J = 0b001,
  MCP9600_THERMOCOUPLE_TYPE_T = 0b010,
  MCP9600_THERMOCOUPLE_TYPE_N = 0b011,
  MCP9600_THERMOCOUPLE_TYPE_S = 0b100,
  MCP9600_THERMOCOUPLE_TYPE_E = 0b101,
  MCP9600_THERMOCOUPLE_TYPE_B = 0b110,
  MCP9600_THERMOCOUPLE_TYPE_R = 0b111,
};

class MCP9600Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_hot_junction(sensor::Sensor *hot_junction) { this->hot_junction_sensor_ = hot_junction; }
  void set_cold_junction(sensor::Sensor *cold_junction) { this->cold_junction_sensor_ = cold_junction; }
  void set_thermocouple_type(MCP9600ThermocoupleType thermocouple_type) {
    this->thermocouple_type_ = thermocouple_type;
  };

 protected:
  uint8_t device_id_{0};

  sensor::Sensor *hot_junction_sensor_{nullptr};
  sensor::Sensor *cold_junction_sensor_{nullptr};

  MCP9600ThermocoupleType thermocouple_type_{MCP9600_THERMOCOUPLE_TYPE_K};

  enum ErrorCode {
    NONE,
    COMMUNICATION_FAILED,
    FAILED_TO_UPDATE_CONFIGURATION,
  } error_code_{NONE};
};

}  // namespace mcp9600
}  // namespace esphome
