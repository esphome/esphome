#pragma once

#include <chrono>
#include "esphome/components/sensor/sensor.h"
#include "haier_base.h"

namespace esphome {
namespace haier {

enum class AirflowVerticalDirection : uint8_t {
  UP = 0,
  CENTER = 1,
  DOWN = 2,
};

enum class AirflowHorizontalDirection : uint8_t {
  LEFT = 0,
  CENTER = 1,
  RIGHT = 2,
};

class HonClimate : public HaierClimateBase {
 public:
  HonClimate() = delete;
  HonClimate(const HonClimate &) = delete;
  HonClimate &operator=(const HonClimate &) = delete;
  HonClimate(esphome::uart::UARTComponent *parent);
  ~HonClimate();
  void dump_config() override;
  void set_beeper_state(bool state);
  bool get_beeper_state() const;
  void set_outdoor_temperature_sensor(esphome::sensor::Sensor *sensor);
  AirflowVerticalDirection get_vertical_airflow() const;
  void set_vertical_airflow(AirflowVerticalDirection direction);
  AirflowHorizontalDirection get_horizontal_airflow() const;
  void set_horizontal_airflow(AirflowHorizontalDirection direction);
  bool get_self_cleaning_status() const;
  void start_self_cleaning();

 protected:
  void set_answers_handlers() override;
  void process_phase(std::chrono::steady_clock::time_point now) override;
  haier_protocol::HaierMessage get_control_message() override;
  bool is_message_invalid(uint8_t message_type) override;

  // Answers handlers
  haier_protocol::HandlerError get_device_version_answer_handler_(uint8_t request_type, uint8_t message_type,
                                                                  const uint8_t *data, size_t data_size);
  haier_protocol::HandlerError get_device_id_answer_handler_(uint8_t request_type, uint8_t message_type,
                                                             const uint8_t *data, size_t data_size);
  haier_protocol::HandlerError status_handler_(uint8_t request_type, uint8_t message_type, const uint8_t *data,
                                               size_t data_size);
  haier_protocol::HandlerError get_management_information_answer_handler_(uint8_t request_type, uint8_t message_type,
                                                                          const uint8_t *data, size_t data_size);
  haier_protocol::HandlerError report_network_status_answer_handler_(uint8_t request_type, uint8_t message_type,
                                                                     const uint8_t *data, size_t data_size);
  haier_protocol::HandlerError get_alarm_status_answer_handler_(uint8_t request_type, uint8_t message_type,
                                                                const uint8_t *data, size_t data_size);
  // Helper functions
  haier_protocol::HandlerError process_status_message_(const uint8_t *packet, uint8_t size);
  std::unique_ptr<uint8_t[]> last_status_message_;
  bool beeper_status_;
  bool self_cleaning_status_;
  bool self_cleaning_start_request_;
  bool got_valid_outdoor_temp_;
  AirflowVerticalDirection vertical_direction_;
  AirflowHorizontalDirection horizontal_direction_;
  bool hvac_hardware_info_available_;
  std::string hvac_protocol_version_;
  std::string hvac_software_version_;
  std::string hvac_hardware_version_;
  std::string hvac_device_name_;
  bool hvac_functions_[5];
  bool &use_crc_;
  uint8_t active_alarms_[8];
  esphome::sensor::Sensor *outdoor_sensor_;
#ifdef HAIER_REPORT_WIFI_SIGNAL
  std::chrono::steady_clock::time_point last_signal_request_;  // To send WiFI signal level
#endif
};

}  // namespace haier
}  // namespace esphome
