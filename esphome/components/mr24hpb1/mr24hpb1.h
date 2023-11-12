#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include <cstdio>
#include <tuple>
#include <vector>

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "packet.h"
#include "types.h"

namespace esphome {
namespace mr24hpb1 {
static const char *const TAG = "mr24hpb1";

const char *forced_unoccupied_to_string(ForcedUnoccupied value);

class MR24HPB1Component : public Component, public uart::UARTDevice {
 public:
  MR24HPB1Component() = default;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

#ifdef USE_TEXT_SENSOR
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
#endif

#ifdef USE_BINARY_SENSOR
  void set_occupancy_sensor(binary_sensor::BinarySensor *sensor) { this->occupancy_sensor_ = sensor; }
  void set_movement_sensor(binary_sensor::BinarySensor *sensor) { this->movement_sensor_ = sensor; }
#endif

#ifdef USE_SENSOR
  void set_movement_rate_sensor(sensor::Sensor *sensor) { this->movement_rate_sensor_ = sensor; }
#endif

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

  bool wait_for_packet_(Packet &packet, uint8_t function_code, uint8_t address_code_1, uint8_t address_code_2,
                        uint8_t timeout_s);

  ReceptionStatus receive_packet_(Packet &packet);

#ifdef USE_TEXT_SENSOR
  void get_general_infos_();
#endif
  void handle_active_reporting_(Packet &packet);
  void handle_passive_reporting_(Packet &packet);
  void handle_sleep_data_report_(Packet &packet);
  void handle_fall_data_report_(Packet &packet);
  void handle_radar_report_(Packet &packet);
#ifdef USE_TEXT_SENSOR
  void handle_module_id_report_(Packet &packet);
#endif
  void handle_other_information_(Packet &packet);
  void handle_system_report_(Packet &packet);
  void handle_other_function_report_(Packet &packet);

  Packet current_packet_;
  ReceptionStatus current_receive_status_ = WAITING;
  uint16_t expected_length_ = 0;

#ifdef USE_TEXT_SENSOR
  uint32_t response_requested_ = 0;
  bool info_fully_populated_ = false;

  std::vector<std::tuple<text_sensor::TextSensor *, AddressCode1, AddressCode2>> system_information_sensors_;

  text_sensor::TextSensor *device_id_sensor_{nullptr};
  text_sensor::TextSensor *software_version_sensor_{nullptr};
  text_sensor::TextSensor *hardware_version_sensor_{nullptr};
  text_sensor::TextSensor *protocol_version_sensor_{nullptr};
  text_sensor::TextSensor *environment_status_sensor_{nullptr};
  text_sensor::TextSensor *movement_type_sensor_{nullptr};
#endif

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *occupancy_sensor_{nullptr};
  binary_sensor::BinarySensor *movement_sensor_{nullptr};
#endif

#ifdef USE_SENSOR
  sensor::Sensor *movement_rate_sensor_{nullptr};
#endif

  SceneSetting scene_setting_;
  uint8_t threshold_gear_;
  ForcedUnoccupied forced_unoccupied_;
};
}  // namespace mr24hpb1
}  // namespace esphome
