#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>

namespace esphome {
namespace mr24hpb1 {
static const char *const TAG = "mr24hpb1";

typedef uint8_t FunctionCode;
typedef uint8_t AddressCode1;
typedef uint8_t AddressCode2;

enum SceneSetting {
  SCENE_DEFAULT = 0x00,
  AREA = 0x01,
  BATHROOM = 0x02,
  BEDROOM = 0x03,
  LIVING_ROOM = 0x04,
  OFFICE = 0x05,
  HOTEL = 0x06
};

const char *SceneSetting_to_string(SceneSetting setting);

enum EnvironmentStatus { UNOCCUPIED = 0x00FFFF, STATIONARY = 0x0100FF, MOVING = 0x010101 };

const char *EnvironmentStatus_to_string(EnvironmentStatus status);

FunctionCode get_packet_function_code(std::vector<uint8_t> &packet);
AddressCode1 get_packet_address_code_1(std::vector<uint8_t> &packet);
AddressCode2 get_packet_address_code_2(std::vector<uint8_t> &packet);
uint16_t get_packet_length(std::vector<uint8_t> &packet);
uint16_t get_packet_crc(std::vector<uint8_t> &packet);
uint32_t packet_data_to_int(std::vector<uint8_t> &packet);
std::vector<uint8_t> get_packet_data(std::vector<uint8_t> &packet);
std::string packet_data_to_string(std::vector<uint8_t> &packet);
float packet_data_to_float(std::vector<uint8_t> &packet);

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
  void set_hardware_version_sensor(text_sensor::TextSensor *hardware_version_sensor) {
    this->hardware_version_sensor_ = hardware_version_sensor;
  }
  void set_protocol_version_sensor(text_sensor::TextSensor *protocol_version_sensor) {
    this->protocol_version_sensor_ = protocol_version_sensor;
  }
  void set_environment_status_sensor(text_sensor::TextSensor *environment_sensor) {
    this->environment_status_sensor_ = environment_sensor;
  }
  void set_occupancy_sensor(binary_sensor::BinarySensor *sensor) { this->occupancy_sensor_ = sensor; }
  void set_movement_sensor(sensor::Sensor *sensor) { this->movement_sensor_ = sensor; }

  void set_scene_setting(SceneSetting setting) { this->scene_setting_ = setting; };
  void set_threshold_gear(uint8_t gear) { this->threshold_gear_ = gear; };

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
  /**
   * @brief Reads the hardware version from the device.
   *
   * @return std::string The hardware version or an empty string if the version could not be read.
   */
  std::string read_hardware_version();
  /**
   * @brief Reads the protocol version from the device.
   *
   * @return std::string The protocol version or an empty string if the version could not be read.
   */
  std::string read_protocol_version();

  void write_threshold_gear(uint8_t gear);
  void write_scene_setting(SceneSetting setting);

  enum reception_status { WAITING, RECEIVING, MALFORMED_PACKET, CRC_ERROR, COMPLETE };
  void write_packet(FunctionCode function_code, AddressCode1 address_code_1, AddressCode2 address_code_2,
                    std::vector<uint8_t> &data);
  void write_packet(FunctionCode function_code, AddressCode1 address_code_1, AddressCode2 address_code_2);

  bool wait_for_packet(std::vector<uint8_t> &packet, uint8_t function_code, uint8_t address_code_1,
                       uint8_t address_code_2, uint8_t timeout_s);

  reception_status receive_packet(std::vector<uint8_t> &packet);

  void log_packet(std::vector<uint8_t> &packet);

  void get_general_infos();
  void handle_active_reporting(std::vector<uint8_t> &packet);
  void handle_passive_reporting(std::vector<uint8_t> &packet);
  void handle_radar_report(std::vector<uint8_t> &packet);
  void handle_module_id_report(std::vector<uint8_t> &packet);
  void handle_other_information(std::vector<uint8_t> &packet);
  void handle_system_report(std::vector<uint8_t> &packet);

  std::vector<uint8_t> current_packet;
  reception_status current_receive_status = WAITING;
  uint32_t respone_requested = 0;
  bool info_fully_populated = false;
  uint16_t expected_length = 0;

  text_sensor::TextSensor *device_id_sensor_{nullptr};
  text_sensor::TextSensor *software_version_sensor_{nullptr};
  text_sensor::TextSensor *hardware_version_sensor_{nullptr};
  text_sensor::TextSensor *protocol_version_sensor_{nullptr};
  text_sensor::TextSensor *environment_status_sensor_{nullptr};
  binary_sensor::BinarySensor *occupancy_sensor_{nullptr};
  sensor::Sensor *movement_sensor_{nullptr};

  SceneSetting scene_setting_;
  uint8_t threshold_gear_;
};
}  // namespace mr24hpb1
}  // namespace esphome