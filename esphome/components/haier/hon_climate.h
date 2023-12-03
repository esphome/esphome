#pragma once

#include <chrono>
#include "esphome/components/sensor/sensor.h"
#include "haier_base.h"

namespace esphome {
namespace haier {

enum class AirflowVerticalDirection : uint8_t {
  HEALTH_UP = 0,
  MAX_UP = 1,
  UP = 2,
  CENTER = 3,
  DOWN = 4,
  HEALTH_DOWN = 5,
};

enum class AirflowHorizontalDirection : uint8_t {
  MAX_LEFT = 0,
  LEFT = 1,
  CENTER = 2,
  RIGHT = 3,
  MAX_RIGHT = 4,
};

enum class CleaningState : uint8_t {
  NO_CLEANING = 0,
  SELF_CLEAN = 1,
  STERI_CLEAN = 2,
};

enum class HonControlMethod { MONITOR_ONLY = 0, SET_GROUP_PARAMETERS, SET_SINGLE_PARAMETER };

class HonClimate : public HaierClimateBase {
 public:
  HonClimate();
  HonClimate(const HonClimate &) = delete;
  HonClimate &operator=(const HonClimate &) = delete;
  ~HonClimate();
  void dump_config() override;
  void set_beeper_state(bool state);
  bool get_beeper_state() const;
  void set_outdoor_temperature_sensor(esphome::sensor::Sensor *sensor);
  AirflowVerticalDirection get_vertical_airflow() const;
  void set_vertical_airflow(AirflowVerticalDirection direction);
  AirflowHorizontalDirection get_horizontal_airflow() const;
  void set_horizontal_airflow(AirflowHorizontalDirection direction);
  std::string get_cleaning_status_text() const;
  CleaningState get_cleaning_status() const;
  void start_self_cleaning();
  void start_steri_cleaning();
  void set_extra_control_packet_bytes_size(size_t size) { this->extra_control_packet_bytes_ = size; };
  void set_control_method(HonControlMethod method) { this->control_method_ = method; };

 protected:
  void set_handlers() override;
  void process_phase(std::chrono::steady_clock::time_point now) override;
  haier_protocol::HaierMessage get_control_message() override;
  haier_protocol::HaierMessage get_power_message(bool state) override;
  bool prepare_pending_action() override;
  void process_protocol_reset() override;

  // Answers handlers
  haier_protocol::HandlerError get_device_version_answer_handler_(haier_protocol::FrameType request_type,
                                                                  haier_protocol::FrameType message_type,
                                                                  const uint8_t *data, size_t data_size);
  haier_protocol::HandlerError get_device_id_answer_handler_(haier_protocol::FrameType request_type,
                                                             haier_protocol::FrameType message_type,
                                                             const uint8_t *data, size_t data_size);
  haier_protocol::HandlerError status_handler_(haier_protocol::FrameType request_type,
                                               haier_protocol::FrameType message_type, const uint8_t *data,
                                               size_t data_size);
  haier_protocol::HandlerError get_management_information_answer_handler_(haier_protocol::FrameType request_type,
                                                                          haier_protocol::FrameType message_type,
                                                                          const uint8_t *data, size_t data_size);
  haier_protocol::HandlerError get_alarm_status_answer_handler_(haier_protocol::FrameType request_type,
                                                                haier_protocol::FrameType message_type,
                                                                const uint8_t *data, size_t data_size);
  // Helper functions
  haier_protocol::HandlerError process_status_message_(const uint8_t *packet, uint8_t size);
  void fill_control_messages_queue_();
  void clear_control_messages_queue_();

  struct HardwareInfo {
    std::string protocol_version_;
    std::string software_version_;
    std::string hardware_version_;
    std::string device_name_;
    bool functions_[5];
  };

  bool beeper_status_;
  CleaningState cleaning_status_;
  bool got_valid_outdoor_temp_;
  AirflowVerticalDirection vertical_direction_;
  AirflowHorizontalDirection horizontal_direction_;
  esphome::optional<HardwareInfo> hvac_hardware_info_;
  uint8_t active_alarms_[8];
  int extra_control_packet_bytes_;
  HonControlMethod control_method_;
  esphome::sensor::Sensor *outdoor_sensor_;
  std::queue<haier_protocol::HaierMessage> control_messages_queue_;
};

}  // namespace haier
}  // namespace esphome
