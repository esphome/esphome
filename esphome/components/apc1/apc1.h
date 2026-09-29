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

class APC1Component final : public uart::UARTDevice, public Component {
  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_10_0)
  SUB_SENSOR(pm_1_0_std)
  SUB_SENSOR(pm_2_5_std)
  SUB_SENSOR(pm_10_0_std)

  SUB_SENSOR(pm_0_3um)
  SUB_SENSOR(pm_0_5um)
  SUB_SENSOR(pm_1_0um)
  SUB_SENSOR(pm_2_5um)
  SUB_SENSOR(pm_5_0um)
  SUB_SENSOR(pm_10_0um)

  SUB_SENSOR(tvoc)
  SUB_SENSOR(eco2)
  SUB_SENSOR(aqi)

  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(raw_temperature)
  SUB_SENSOR(raw_humidity)

  SUB_SENSOR(rs0)
  SUB_SENSOR(rs2)
  SUB_SENSOR(rs3)

  SUB_SENSOR(error_code)

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

 protected:
  void send_command_(APC1Command cmd, uint16_t data);
  void initialize_device_();
  void parse_frame_(uint16_t total_len);
  void parse_measurement_frame_();
  void parse_device_info_frame_();
  void parse_command_response_frame_();

  GPIOPin *set_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  uint32_t last_transmission_{0};
  uint32_t last_valid_frame_{0};
  uint32_t last_init_attempt_{0};
  std::array<uint8_t, APC1_BUFFER_SIZE> rx_buffer_{};
  uint8_t rx_index_{0};
  uint8_t last_error_code_{0xFF};
  bool active_mode_{true};
  bool idle_{false};
  bool gas_warming_up_{false};
};

}  // namespace esphome::apc1
