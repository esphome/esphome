#pragma once

#include <chrono>
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/core/automation.h"
#include "haier_base.h"
#include "hon_packet.h"

namespace esphome {
namespace haier {

enum class CleaningState : uint8_t {
  NO_CLEANING = 0,
  SELF_CLEAN = 1,
  STERI_CLEAN = 2,
};

enum class HonControlMethod { MONITOR_ONLY = 0, SET_GROUP_PARAMETERS, SET_SINGLE_PARAMETER };

struct HonSettings {
  hon_protocol::VerticalSwingMode last_vertiacal_swing;
  hon_protocol::HorizontalSwingMode last_horizontal_swing;
};

class HonClimate : public HaierClimateBase {
#ifdef USE_SENSOR
 public:
  enum class SubSensorType {
    // Used data based sensors
    OUTDOOR_TEMPERATURE = 0,
    HUMIDITY,
    // Big data based sensors
    INDOOR_COIL_TEMPERATURE,
    OUTDOOR_COIL_TEMPERATURE,
    OUTDOOR_DEFROST_TEMPERATURE,
    OUTDOOR_IN_AIR_TEMPERATURE,
    OUTDOOR_OUT_AIR_TEMPERATURE,
    POWER,
    COMPRESSOR_FREQUENCY,
    COMPRESSOR_CURRENT,
    EXPANSION_VALVE_OPEN_DEGREE,
    SUB_SENSOR_TYPE_COUNT,
    BIG_DATA_FRAME_SUB_SENSORS = INDOOR_COIL_TEMPERATURE,
  };
  void set_sub_sensor(SubSensorType type, sensor::Sensor *sens);

 protected:
  void update_sub_sensor_(SubSensorType type, float value);
  sensor::Sensor *sub_sensors_[(size_t) SubSensorType::SUB_SENSOR_TYPE_COUNT]{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
 public:
  enum class SubBinarySensorType {
    OUTDOOR_FAN_STATUS = 0,
    DEFROST_STATUS,
    COMPRESSOR_STATUS,
    INDOOR_FAN_STATUS,
    FOUR_WAY_VALVE_STATUS,
    INDOOR_ELECTRIC_HEATING_STATUS,
    SUB_BINARY_SENSOR_TYPE_COUNT,
  };
  void set_sub_binary_sensor(SubBinarySensorType type, binary_sensor::BinarySensor *sens);

 protected:
  void update_sub_binary_sensor_(SubBinarySensorType type, uint8_t value);
  binary_sensor::BinarySensor *sub_binary_sensors_[(size_t) SubBinarySensorType::SUB_BINARY_SENSOR_TYPE_COUNT]{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
 public:
  enum class SubTextSensorType {
    CLEANING_STATUS = 0,
    PROTOCOL_VERSION,
    APPLIANCE_NAME,
    SUB_TEXT_SENSOR_TYPE_COUNT,
  };
  void set_sub_text_sensor(SubTextSensorType type, text_sensor::TextSensor *sens);

 protected:
  void update_sub_text_sensor_(SubTextSensorType type, const std::string &value);
  text_sensor::TextSensor *sub_text_sensors_[(size_t) SubTextSensorType::SUB_TEXT_SENSOR_TYPE_COUNT]{nullptr};
#endif
 public:
  HonClimate();
  HonClimate(const HonClimate &) = delete;
  HonClimate &operator=(const HonClimate &) = delete;
  ~HonClimate();
  void dump_config() override;
  void set_beeper_state(bool state);
  bool get_beeper_state() const;
  esphome::optional<hon_protocol::VerticalSwingMode> get_vertical_airflow() const;
  void set_vertical_airflow(hon_protocol::VerticalSwingMode direction);
  esphome::optional<hon_protocol::HorizontalSwingMode> get_horizontal_airflow() const;
  void set_horizontal_airflow(hon_protocol::HorizontalSwingMode direction);
  std::string get_cleaning_status_text() const;
  CleaningState get_cleaning_status() const;
  void start_self_cleaning();
  void start_steri_cleaning();
  void set_extra_control_packet_bytes_size(size_t size) { this->extra_control_packet_bytes_ = size; };
  void set_control_method(HonControlMethod method) { this->control_method_ = method; };
  void add_alarm_start_callback(std::function<void(uint8_t, const char *)> &&callback);
  void add_alarm_end_callback(std::function<void(uint8_t, const char *)> &&callback);
  float get_active_alarm_count() const { return this->active_alarm_count_; }

 protected:
  void set_handlers() override;
  void process_phase(std::chrono::steady_clock::time_point now) override;
  haier_protocol::HaierMessage get_control_message() override;
  haier_protocol::HaierMessage get_power_message(bool state) override;
  void initialization() override;
  bool prepare_pending_action() override;
  void process_protocol_reset() override;
  bool should_get_big_data_();

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
  haier_protocol::HandlerError alarm_status_message_handler_(haier_protocol::FrameType type, const uint8_t *buffer,
                                                             size_t size);
  // Helper functions
  haier_protocol::HandlerError process_status_message_(const uint8_t *packet, uint8_t size);
  void process_alarm_message_(const uint8_t *packet, uint8_t size, bool check_new);
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
  esphome::optional<hon_protocol::VerticalSwingMode> pending_vertical_direction_{};
  esphome::optional<hon_protocol::HorizontalSwingMode> pending_horizontal_direction_{};
  esphome::optional<HardwareInfo> hvac_hardware_info_{};
  uint8_t active_alarms_[8];
  int extra_control_packet_bytes_;
  HonControlMethod control_method_;
  std::queue<haier_protocol::HaierMessage> control_messages_queue_;
  CallbackManager<void(uint8_t, const char *)> alarm_start_callback_{};
  CallbackManager<void(uint8_t, const char *)> alarm_end_callback_{};
  float active_alarm_count_{NAN};
  std::chrono::steady_clock::time_point last_alarm_request_;
  int big_data_sensors_{0};
  esphome::optional<hon_protocol::VerticalSwingMode> current_vertical_swing_{};
  esphome::optional<hon_protocol::HorizontalSwingMode> current_horizontal_swing_{};
  HonSettings settings_;
  ESPPreferenceObject rtc_;
};

class HaierAlarmStartTrigger : public Trigger<uint8_t, const char *> {
 public:
  explicit HaierAlarmStartTrigger(HonClimate *parent) {
    parent->add_alarm_start_callback(
        [this](uint8_t alarm_code, const char *alarm_message) { this->trigger(alarm_code, alarm_message); });
  }
};

class HaierAlarmEndTrigger : public Trigger<uint8_t, const char *> {
 public:
  explicit HaierAlarmEndTrigger(HonClimate *parent) {
    parent->add_alarm_end_callback(
        [this](uint8_t alarm_code, const char *alarm_message) { this->trigger(alarm_code, alarm_message); });
  }
};

}  // namespace haier
}  // namespace esphome
