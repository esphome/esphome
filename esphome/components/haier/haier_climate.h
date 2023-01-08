#pragma once

#include <chrono>
#include <set>
#include "esphome.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "transport/protocol_transport.h"
#include "protocol/haier_protocol.h"

namespace esphome {
namespace haier {

enum class AirflowVerticalDirection : uint8_t
{
  UP      = 0,
  CENTER  = 1,
  DOWN    = 2,
};

enum class AirflowHorizontalDirection : uint8_t
{
  LEFT    = 0,
  CENTER  = 1,
  RIGHT   = 2,
};

class HaierClimate :  public esphome::Component,
      public esphome::climate::Climate,
      public esphome::uart::UARTDevice,
      public haier_protocol::ProtocolStream
{
public:
  HaierClimate() = delete;
  HaierClimate(const HaierClimate&) = delete;
  HaierClimate& operator=(const HaierClimate&) = delete;
  HaierClimate(esphome::uart::UARTComponent* parent);
  ~HaierClimate();
  void setup() override;
  void loop() override;
  void control(const esphome::climate::ClimateCall &call) override;
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::HARDWARE ; }
  void set_beeper_state(bool state);
  bool get_beeper_state() const;   
  void set_fahrenheit(bool fahrenheit);
  void set_outdoor_temperature_sensor(esphome::sensor::Sensor *sensor);
  void set_display_state(bool state);
  bool get_display_state() const;
  AirflowVerticalDirection get_vertical_airflow() const;
  void set_vertical_airflow(AirflowVerticalDirection direction);
  AirflowHorizontalDirection get_horizontal_airflow() const;
  void set_horizontal_airflow(AirflowHorizontalDirection direction);
  void set_supported_swing_modes(const std::set<esphome::climate::ClimateSwingMode> &modes);
  virtual size_t available() noexcept { return esphome::uart::UARTDevice::available(); };
  virtual size_t read_array(uint8_t* data, size_t len) noexcept { return esphome::uart::UARTDevice::read_array(data, len) ? len : 0; };
  virtual void write_array(const uint8_t* data, size_t len) noexcept { esphome::uart::UARTDevice::write_array(data, len);};
  bool can_send_message() const { return haier_protocol_.get_outgoing_queue_size() == 0; };
protected:
  enum class ProtocolPhases
  {
    UNKNOWN                       = -1,
    // INITIALIZATION
    SENDING_INIT_1                = 0,
    WAITING_ANSWER_INIT_1         = 1,
    SENDING_INIT_2                = 2,
    WAITING_ANSWER_INIT_2         = 3,
    SENDING_FIRST_STATUS_REQUEST  = 4,
    WAITING_FIRST_STATUS_ANSWER   = 5,
    SENDING_ALARM_STATUS_REQUEST  = 6,
    WAITING_ALARM_STATUS_ANSWER   = 7,
    // FUNCTIONAL STATE
    IDLE                          = 8,
    SENDING_STATUS_REQUEST        = 9,
    WAITING_STATUS_ANSWER         = 10,
    SENDING_UPDATE_SIGNAL_REQUEST = 11,
    WAITING_UPDATE_SIGNAL_ANSWER  = 12,
    SENDING_SIGNAL_LEVEL          = 13,
    WAITING_SIGNAL_LEVEL_ANSWER   = 14,
    SENDING_CONTROL               = 15,
    WAITING_CONTROL_ANSWER        = 16,
    NUM_PROTOCOL_PHASES
  };
#if (HAIER_LOG_LEVEL > 4)
  const char* phase_to_string_(ProtocolPhases phase);
#endif
  esphome::climate::ClimateTraits traits() override;
  // Answers handlers
  haier_protocol::HandlerError answer_preprocess(uint8_t requestMessageType, uint8_t expectedRequestMessageType, uint8_t answerMessageType, uint8_t expectedAnswerMessageType, ProtocolPhases expectedPhase);
  haier_protocol::HandlerError get_device_version_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
  haier_protocol::HandlerError get_device_id_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
  haier_protocol::HandlerError status_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
  haier_protocol::HandlerError get_management_information_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
  haier_protocol::HandlerError report_network_status_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
  haier_protocol::HandlerError get_alarm_status_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize);
  // Timeout handler
  haier_protocol::HandlerError timeout_default_handler(uint8_t requestType);
  // Helper functions
  haier_protocol::HandlerError process_status_message(const uint8_t* packet, uint8_t size);
  void send_message(const haier_protocol::HaierMessage& command);
  const haier_protocol::HaierMessage get_control_message();
  void set_phase(ProtocolPhases phase);
protected:
  struct HvacSettings
  {
    esphome::optional<esphome::climate::ClimateMode> mode;
    esphome::optional<esphome::climate::ClimateFanMode> fan_mode;
    esphome::optional<esphome::climate::ClimateSwingMode> swing_mode;
    esphome::optional<float> target_temperature;
    esphome::optional<esphome::climate::ClimatePreset> preset;
    bool valid;
    HvacSettings() : valid(false) {};
    void reset();
  };
  haier_protocol::ProtocolHandler       haier_protocol_;
  ProtocolPhases                        protocol_phase_;
  uint8_t*                              last_status_message_;
  uint8_t                               fan_mode_speed_;
  uint8_t                               other_modes_fan_speed_;
  bool                                  beeper_status_;
  bool                                  display_status_;
  bool                                  force_send_control_;
  bool                                  forced_publish_;
  bool                                  forced_request_status_;
  bool                                  control_called_;
  bool                                  got_valid_outdoor_temp_;
  AirflowVerticalDirection              vertical_direction_;
  AirflowHorizontalDirection            horizontal_direction_;
  bool                                  hvac_hardware_info_available_;
  std::string                           hvac_protocol_version_;
  std::string                           hvac_software_version_;
  std::string                           hvac_hardware_version_;
  std::string                           hvac_device_name_;
  bool                                  hvac_functions_[5];
  bool&                                 use_crc_;
  uint8_t                               active_alarms_[8];
  esphome::sensor::Sensor*              outdoor_sensor_;
  esphome::climate::ClimateTraits       traits_;
  HvacSettings                          hvac_settings_;
  std::chrono::steady_clock::time_point last_request_timestamp_;      // For interval between messages
  std::chrono::steady_clock::time_point last_valid_status_timestamp_; // For protocol timeout
  std::chrono::steady_clock::time_point last_status_request_;         // To request AC status
#ifdef HAIER_REPORT_WIFI_SIGNAL
  std::chrono::steady_clock::time_point last_signal_request_;         // To send WiFI signal level
#endif
  std::chrono::steady_clock::time_point control_request_timestamp_;   // To send control message
};

} // namespace haier
} // namespace esphome
