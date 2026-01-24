#pragma once

#include <optional>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace adafruit_seesaw_soil {

class AdafruitSeesawSoil : public PollingComponent, public i2c::I2CDevice {
 public:
  struct Version {
    uint16_t pid{0};
    uint8_t year{0};
    uint8_t month{0};
    uint8_t day{0};
  };

  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  std::optional<Version> get_version();
  std::optional<float> get_temperature_c();
  std::optional<uint16_t> get_moisture();

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_moisture_sensor(sensor::Sensor *moisture_sensor) { moisture_sensor_ = moisture_sensor; }

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *moisture_sensor_{nullptr};
  std::optional<Version> version_;
  uint8_t hardware_type_{0};

 private:
  enum class LoopState : uint8_t {
    BOOT,
    RESET_COMMAND_SENT,
    HW_ID_COMMAND_SENT,
    HW_ID_RESPONSE_READ,
    WAITING_TO_START_READING,
    SETUP_FAILED,
    WAITING_TO_UPDATE_TEMP,
    READ_TEMP_COMMAND_SENT,
    WAITING_TO_UPDATE_MOIST,
    READ_MOIST_COMMAND_SENT,
  };

  LoopState loop_state_{LoopState::BOOT};
  unsigned setup_retry_count_{0};
  unsigned last_setup_op_{0};
  unsigned moisture_read_count_{0};
  unsigned last_moisture_read_op_{0};
  unsigned temperature_read_count_{0};
  unsigned last_temperature_read_op_{0};
};

}  // namespace adafruit_seesaw_soil
}  // namespace esphome
