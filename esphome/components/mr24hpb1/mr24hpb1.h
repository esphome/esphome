#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <vector>

namespace esphome {
namespace mr24hpb1 {
typedef uint8_t FunctionCode;
typedef uint8_t AddressCode1;
typedef uint8_t AddressCode2;

class MR24HPB1Component : public Component, public uart::UARTDevice {
 public:
  MR24HPB1Component() = default;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

  void set_device_id_sensor(text_sensor::TextSensor *device_id_sensor) { this->device_id_sensor_ = device_id_sensor; }
  void set_software_version_sensor(text_sensor::TextSensor *software_version_sensor) {
    this->software_version_sensor_ = software_version_sensor;
  }

 protected:
 /**
  * @brief Reads the device ID from the device.
  *
  * @return std::string The device ID or an empty string if the ID could not be read.
  */
  std::string read_device_id();
  /**
   * @brief Reads the software version from the device.
   *
   * @return std::string The software version or an empty string if the version could not be read.
   */
  std::string read_software_version();

  enum reception_status { WAITING, RECEIVING, MALFORMED_PACKET, CRC_ERROR, COMPLETE };
  void write_packet(FunctionCode function_code, AddressCode1 address_code_1, AddressCode2 address_code_2,
                    std::vector<uint8_t> &data);
  void write_packet(FunctionCode function_code, AddressCode1 address_code_1, AddressCode2 address_code_2);

  bool wait_for_packet(std::vector<uint8_t> &packet, uint8_t function_code, uint8_t address_code_1,
                       uint8_t address_code_2, uint8_t timeout_s);

  reception_status receive_packet(std::vector<uint8_t> &packet);

  void log_packet(std::vector<uint8_t> &packet);

  std::vector<uint8_t> packet;
  reception_status current_receive_status = WAITING;
  bool reading = false;
  uint16_t expected_length = 0;

  text_sensor::TextSensor *device_id_sensor_{nullptr};
  text_sensor::TextSensor *software_version_sensor_{nullptr};
};
}  // namespace mr24hpb1
}  // namespace esphome