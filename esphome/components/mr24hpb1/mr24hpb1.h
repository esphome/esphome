#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include <cstdio>
#include <tuple>
#include <vector>

namespace esphome {
namespace mr24hpb1 {
static const char *const TAG = "mr24hpb1";

using FunctionCode = uint8_t;
using AddressCode1 = uint8_t;
using AddressCode2 = uint8_t;

enum SceneSetting {
  SCENE_DEFAULT = 0x00,
  AREA = 0x01,
  BATHROOM = 0x02,
  BEDROOM = 0x03,
  LIVING_ROOM = 0x04,
  OFFICE = 0x05,
  HOTEL = 0x06
};

const char *scene_setting_to_string(SceneSetting setting);

enum EnvironmentStatus { UNOCCUPIED = 0x00FFFF, STATIONARY = 0x0100FF, MOVING = 0x010101 };

const char *environment_status_to_string(EnvironmentStatus status);

enum class MovementType { NONE = 0x01, APPROACHING = 0x02, FAR_AWAY = 0x03, U1 = 0x04, U2 = 0x05 };

const char *movement_type_to_string(MovementType type);

enum class ForcedUnoccupied {
  NONE = 0x00,
  SEC_10 = 0x01,
  SEC_30 = 0x02,
  MIN_1 = 0x03,
  MIN_2 = 0x04,
  MIN_5 = 0x05,
  MIN_10 = 0x06,
  MIN_30 = 0x07,
  MIN_60 = 0x08
};

enum class BreathingSigns {
  NORMAL = 0x00,
  BREATHING_ABNORMALLY = 0x01,
  NO_SIGNAL = 0x02,
  MOVEMENT_ANOMALY = 0x04,
  SHORTNESS_OF_BREATH = 0x05
};
enum class BedOccupation { OUT_OF_BED = 0x00, IN_BED = 0x01, NA = 0x02 };
enum class SleepState { AWAKE = 0x00, LIGHT_SLEEP = 0x01, DEEP_SLEEP = 0x02, NA = 0x03 };

const char *forced_unoccupied_to_string(ForcedUnoccupied value);

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
  void set_movement_type_sensor(text_sensor::TextSensor *sensor) { this->movement_type_sensor_ = sensor; }
  void set_occupancy_sensor(binary_sensor::BinarySensor *sensor) { this->occupancy_sensor_ = sensor; }
  void set_movement_rate_sensor(sensor::Sensor *sensor) { this->movement_rate_sensor_ = sensor; }
  void set_movement_sensor(binary_sensor::BinarySensor *sensor) { this->movement_sensor_ = sensor; }

  void set_scene_setting(SceneSetting setting) { this->scene_setting_ = setting; };
  void set_forced_unoccupied(ForcedUnoccupied setting) { this->forced_unoccupied_ = setting; };
  void set_threshold_gear(uint8_t gear) { this->threshold_gear_ = gear; };

 protected:
  /**
   * @brief Reads the device ID from the device.
   *
   * @return std::string The device ID or an empty string if the ID could not be read.
   */
  std::string read_device_id_();
  /**
   * @brief Reads the software version from the device.
   *
   * @return std::string The software version or an empty string if the version could not be read.
   */
  std::string read_software_version_();
  /**
   * @brief Reads the hardware version from the device.
   *
   * @return std::string The hardware version or an empty string if the version could not be read.
   */
  std::string read_hardware_version_();
  /**
   * @brief Reads the protocol version from the device.
   *
   * @return std::string The protocol version or an empty string if the version could not be read.
   */
  std::string read_protocol_version_();

  void write_threshold_gear_(uint8_t gear);
  void write_scene_setting_(SceneSetting setting);
  void write_force_unoccupied_setting_(ForcedUnoccupied setting);

  enum ReceptionStatus { WAITING, RECEIVING, MALFORMED_PACKET, CRC_ERROR, COMPLETE };
  void write_packet_(FunctionCode function_code, AddressCode1 address_code_1, AddressCode2 address_code_2,
                     std::vector<uint8_t> &data);
  void write_packet_(FunctionCode function_code, AddressCode1 address_code_1, AddressCode2 address_code_2);

  bool wait_for_packet_(std::vector<uint8_t> &packet, uint8_t function_code, uint8_t address_code_1,
                        uint8_t address_code_2, uint8_t timeout_s);

  ReceptionStatus receive_packet_(std::vector<uint8_t> &packet);

  void log_packet_(std::vector<uint8_t> &packet);

  void get_general_infos_();
  void handle_active_reporting_(std::vector<uint8_t> &packet);
  void handle_passive_reporting_(std::vector<uint8_t> &packet);
  void handle_sleep_data_report_(std::vector<uint8_t> &packet);
  void handle_fall_data_report_(std::vector<uint8_t> &current_packet);
  void handle_radar_report_(std::vector<uint8_t> &packet);
  void handle_module_id_report_(std::vector<uint8_t> &packet);
  void handle_other_information_(std::vector<uint8_t> &packet);
  void handle_system_report_(std::vector<uint8_t> &packet);
  void handle_other_function_report_(std::vector<uint8_t> &packet);

  std::vector<uint8_t> current_packet_;
  ReceptionStatus current_receive_status_ = WAITING;
  uint32_t respone_requested_ = 0;
  bool info_fully_populated_ = false;
  uint16_t expected_length_ = 0;
  std::vector<std::tuple<text_sensor::TextSensor *, AddressCode1, AddressCode2>> system_information_sensors_;

  text_sensor::TextSensor *device_id_sensor_{nullptr};
  text_sensor::TextSensor *software_version_sensor_{nullptr};
  text_sensor::TextSensor *hardware_version_sensor_{nullptr};
  text_sensor::TextSensor *protocol_version_sensor_{nullptr};
  text_sensor::TextSensor *environment_status_sensor_{nullptr};
  binary_sensor::BinarySensor *occupancy_sensor_{nullptr};
  binary_sensor::BinarySensor *movement_sensor_{nullptr};
  sensor::Sensor *movement_rate_sensor_{nullptr};
  text_sensor::TextSensor *movement_type_sensor_{nullptr};

  SceneSetting scene_setting_;
  uint8_t threshold_gear_;
  ForcedUnoccupied forced_unoccupied_;
};
}  // namespace mr24hpb1
}  // namespace esphome
