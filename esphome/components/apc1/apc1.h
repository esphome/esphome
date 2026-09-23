#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

#include <array>

namespace esphome::apc1 {

static constexpr uint8_t APC1_HEADER_1 = 0x42;
static constexpr uint8_t APC1_HEADER_2 = 0x4D;
static constexpr uint8_t APC1_FRAME_HEADER_SIZE = 4;  // 2 start bytes (0x42, 0x4D) + 2 frame length bytes
static constexpr uint8_t APC1_FRAME_SIZE_MEASUREMENT = 64;
static constexpr uint8_t APC1_FRAME_SIZE_DEVICE_INFO = 23;
static constexpr uint8_t APC1_FRAME_SIZE_COMMAND_RESPONSE = 8;
static constexpr uint8_t APC1_COMMAND_FRAME_SIZE = 7;
static constexpr size_t APC1_BUFFER_SIZE = APC1_FRAME_SIZE_MEASUREMENT;

enum class APC1Command : uint8_t {
  APC1_COMMAND_MEASUREMENT_MODE = 0xE1,
  APC1_COMMAND_REQUEST_MEASUREMENT = 0xE2,
  APC1_COMMAND_OPERATION_MODE = 0xE4,
  APC1_COMMAND_READ_SENSOR_VERSION = 0xE9,
};

static constexpr uint16_t APC1_MEASUREMENT_MODE_PASSIVE = 0x0000;
static constexpr uint16_t APC1_MEASUREMENT_MODE_ACTIVE = 0x0001;

static constexpr uint16_t APC1_OPERATING_MODE_IDLE = 0x0000;
static constexpr uint16_t APC1_OPERATING_MODE_STANDARD = 0x0001;

class APC1Component : public uart::UARTDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_active_mode();
  void set_passive_mode();
  void request_measurement();
  void set_idle_mode();
  void set_measurement_mode();

  void set_set_pin(GPIOPin *set_pin) { this->set_pin_ = set_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }

  void set_pm_1_0_sensor(sensor::Sensor *sensor) { this->pm_1_0_sensor_ = sensor; }
  void set_pm_2_5_sensor(sensor::Sensor *sensor) { this->pm_2_5_sensor_ = sensor; }
  void set_pm_10_0_sensor(sensor::Sensor *sensor) { this->pm_10_0_sensor_ = sensor; }
  void set_pm_1_0_std_sensor(sensor::Sensor *sensor) { this->pm_1_0_std_sensor_ = sensor; }
  void set_pm_2_5_std_sensor(sensor::Sensor *sensor) { this->pm_2_5_std_sensor_ = sensor; }
  void set_pm_10_0_std_sensor(sensor::Sensor *sensor) { this->pm_10_0_std_sensor_ = sensor; }

  void set_pm_0_3um_sensor(sensor::Sensor *sensor) { this->pm_0_3um_sensor_ = sensor; }
  void set_pm_0_5um_sensor(sensor::Sensor *sensor) { this->pm_0_5um_sensor_ = sensor; }
  void set_pm_1_0um_sensor(sensor::Sensor *sensor) { this->pm_1_0um_sensor_ = sensor; }
  void set_pm_2_5um_sensor(sensor::Sensor *sensor) { this->pm_2_5um_sensor_ = sensor; }
  void set_pm_5_0um_sensor(sensor::Sensor *sensor) { this->pm_5_0um_sensor_ = sensor; }
  void set_pm_10_0um_sensor(sensor::Sensor *sensor) { this->pm_10_0um_sensor_ = sensor; }

  void set_tvoc_sensor(sensor::Sensor *sensor) { this->tvoc_sensor_ = sensor; }
  void set_eco2_sensor(sensor::Sensor *sensor) { this->eco2_sensor_ = sensor; }
  void set_aqi_sensor(sensor::Sensor *sensor) { this->aqi_sensor_ = sensor; }

  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_raw_temperature_sensor(sensor::Sensor *sensor) { this->raw_temperature_sensor_ = sensor; }
  void set_raw_humidity_sensor(sensor::Sensor *sensor) { this->raw_humidity_sensor_ = sensor; }

  void set_rs0_sensor(sensor::Sensor *sensor) { this->rs0_sensor_ = sensor; }
  void set_rs2_sensor(sensor::Sensor *sensor) { this->rs2_sensor_ = sensor; }
  void set_rs3_sensor(sensor::Sensor *sensor) { this->rs3_sensor_ = sensor; }

  void set_error_code_sensor(sensor::Sensor *sensor) { this->error_code_sensor_ = sensor; }

 protected:
  void send_command_(APC1Command cmd, uint16_t data);
  void initialize_device_();
  void parse_frame_(uint16_t total_len);
  void parse_measurement_frame_();
  void parse_device_info_frame_();
  void parse_command_response_frame_();

  std::array<uint8_t, APC1_BUFFER_SIZE> rx_buffer_{};
  uint8_t rx_index_{0};
  uint8_t last_error_code_{0xFF};
  bool active_mode_{true};
  bool gas_warming_up_{false};
  uint32_t last_transmission_{0};
  uint32_t last_update_{0};
  uint32_t last_valid_frame_{0};
  uint32_t last_init_attempt_{0};
  uint32_t update_interval_{0};

  GPIOPin *set_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};

  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  sensor::Sensor *pm_1_0_std_sensor_{nullptr};
  sensor::Sensor *pm_2_5_std_sensor_{nullptr};
  sensor::Sensor *pm_10_0_std_sensor_{nullptr};

  sensor::Sensor *pm_0_3um_sensor_{nullptr};
  sensor::Sensor *pm_0_5um_sensor_{nullptr};
  sensor::Sensor *pm_1_0um_sensor_{nullptr};
  sensor::Sensor *pm_2_5um_sensor_{nullptr};
  sensor::Sensor *pm_5_0um_sensor_{nullptr};
  sensor::Sensor *pm_10_0um_sensor_{nullptr};

  sensor::Sensor *tvoc_sensor_{nullptr};
  sensor::Sensor *eco2_sensor_{nullptr};
  sensor::Sensor *aqi_sensor_{nullptr};

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *raw_temperature_sensor_{nullptr};
  sensor::Sensor *raw_humidity_sensor_{nullptr};

  sensor::Sensor *rs0_sensor_{nullptr};
  sensor::Sensor *rs2_sensor_{nullptr};
  sensor::Sensor *rs3_sensor_{nullptr};

  sensor::Sensor *error_code_sensor_{nullptr};
};

}  // namespace esphome::apc1
