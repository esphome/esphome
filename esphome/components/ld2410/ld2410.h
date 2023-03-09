#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "ld2410_call.h"

namespace esphome {
namespace ld2410 {

#define CHECK_BIT(var, pos) (((var) >> (pos)) & 1)

// Commands
static const uint8_t CMD_ENABLE_CONF = 0x00FF;
static const uint8_t CMD_DISABLE_CONF = 0x00FE;
static const uint8_t CMD_ENABLE_ENG = 0x0062;
static const uint8_t CMD_DISABLE_ENG = 0x0063;
static const uint8_t CMD_MAXDIST_DURATION = 0x0060;
static const uint8_t CMD_QUERY = 0x0061;
static const uint8_t CMD_GATE_SENS = 0x0064;
static const uint8_t CMD_VERSION = 0x00A0;
static const uint8_t CMD_QUERY_DISTANCE_RESOLUTION = 0x00AB;
static const uint8_t CMD_SET_DISTANCE_RESOLUTION = 0x00AA;
static const uint8_t CMD_BT_PASSWORD = 0x00A9;
static const uint8_t CMD_MAC = 0x00A5;
static const uint8_t CMD_RESET = 0x00A2;
static const uint8_t CMD_RESTART = 0x00A3;
static const uint8_t CMD_BLUETOOTH = 0x00A4;

enum DistanceResolution : uint8_t { DISTANCE_RESOLUTION_TWO = 0x01, DISTANCE_RESOLUTION_SEVEN = 0x00 };
// Commands values
static const uint8_t CMD_MAX_MOVE_VALUE = 0x0000;
static const uint8_t CMD_MAX_STILL_VALUE = 0x0001;
static const uint8_t CMD_DURATION_VALUE = 0x0002;
// Command Header & Footer
static const uint8_t CMD_FRAME_HEADER[4] = {0xFD, 0xFC, 0xFB, 0xFA};
static const uint8_t CMD_FRAME_END[4] = {0x04, 0x03, 0x02, 0x01};
// Data Header & Footer
static const uint8_t DATA_FRAME_HEADER[4] = {0xF4, 0xF3, 0xF2, 0xF1};
static const uint8_t DATA_FRAME_END[4] = {0xF8, 0xF7, 0xF6, 0xF5};
/*
Data Type: 6th byte
Target states: 9th byte
    Moving target distance: 10~11th bytes
    Moving target energy: 12th byte
    Still target distance: 13~14th bytes
    Still target energy: 15th byte
    Detect distance: 16~17th bytes
*/
enum PeriodicDataStructure : uint8_t {
  DATA_TYPES = 6,
  TARGET_STATES = 8,
  MOVING_TARGET_LOW = 9,
  MOVING_TARGET_HIGH = 10,
  MOVING_ENERGY = 11,
  STILL_TARGET_LOW = 12,
  STILL_TARGET_HIGH = 13,
  STILL_ENERGY = 14,
  DETECT_DISTANCE_LOW = 15,
  DETECT_DISTANCE_HIGH = 16,
  MOVING_SENSOR_START = 19,
  STILL_SENSOR_START = 28,
};
enum PeriodicDataValue : uint8_t { HEAD = 0XAA, END = 0x55, CHECK = 0x00 };

enum AckDataStructure : uint8_t { COMMAND = 6, COMMAND_STATUS = 7 };

#define SUB_LAMBDA_SELECT(name, cb) \
 protected: \
  select::Select *name##_select_{nullptr}; \
\
 public: \
  void set_##name##_select(select::Select *select) { \
    this->name##_select_ = select; \
    this->name##_select_->add_on_state_callback([this](const std::string &state, size_t size) { cb; }); \
  }

#define SUB_LAMBDA_SWITCH(name, cb) \
 protected: \
  switch_::Switch *name##_switch_{nullptr}; \
\
 public: \
  void set_##name##_switch(switch_::Switch *s) { \
    this->name##_switch_ = s; \
    this->name##_switch_->add_on_state_callback([this](bool state) { cb; }); \
  }

#define SUB_LAMBDA_BUTTON(name, cb) \
 protected: \
  button::Button *name##_button_{nullptr}; \
\
 public: \
  void set_##name##_button(button::Button *b) { \
    this->name##_button_ = b; \
    this->name##_button_->add_on_press_callback([this]() { cb; }); \
  }

#define SUB_LAMBDA_NUMBER(name, cb) \
 protected: \
  number::Number *name##_number_{nullptr}; \
\
 public: \
  void set_##name##_number(number::Number *n) { \
    this->name##_number_ = n; \
    this->name##_number_->add_on_state_callback([this](float state) { \
      this->set_config_mode_(true); \
      cb; \
      this->set_config_mode_(false); \
    }); \
  }

//  char cmd[2] = {enable ? 0xFF : 0xFE, 0x00};
class LD2410Component : public Component, public uart::UARTDevice {
#ifdef USE_SENSOR
  SUB_SENSOR(moving_target_distance)
  SUB_SENSOR(still_target_distance)
  SUB_SENSOR(moving_target_energy)
  SUB_SENSOR(still_target_energy)
  SUB_SENSOR(detection_distance)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(target)
  SUB_BINARY_SENSOR(moving_target)
  SUB_BINARY_SENSOR(still_target)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(version)
  SUB_TEXT_SENSOR(mac)
#endif
#ifdef USE_SELECT
  SUB_LAMBDA_SELECT(distance_resolution, {
    this->set_config_mode_(true);
    this->set_distance_resolution_(state);
    this->set_timeout(200, [this]() {
      this->restart_();
      this->set_timeout(1000, [this]() {
        this->set_config_mode_(true);
        this->query_parameters_();
        this->get_mac_();
        this->get_distance_resolution_();
        this->set_config_mode_(false);
      });
    });
  })
#endif
#ifdef USE_SWITCH
  SUB_LAMBDA_SWITCH(engineering_mode, {
    this->set_config_mode_(true);
    this->set_engineering_mode_(state);
    this->set_config_mode_(false);
  })
  SUB_LAMBDA_SWITCH(bluetooth, {
    this->set_config_mode_(true);
    this->set_bluetooth_(state);
    this->set_timeout(200, [this]() {
      this->set_config_mode_(true);
      this->restart_();
      this->set_timeout(1000, [this]() {
        this->set_config_mode_(true);
        this->query_parameters_();
        this->get_mac_();
        this->set_config_mode_(false);
      });
    });
  })
#endif
#ifdef USE_BUTTON
  SUB_LAMBDA_BUTTON(reset, {
    this->set_config_mode_(true);
    this->factory_reset_();
    this->set_timeout(200, [this]() {
      this->set_config_mode_(true);
      this->restart_();
      this->set_timeout(1000, [this]() {
        this->set_config_mode_(true);
        this->query_parameters_();
        this->get_mac_();
        this->get_distance_resolution_();
        this->set_config_mode_(false);
      });
    });
  })
  SUB_LAMBDA_BUTTON(restart, {
    this->set_config_mode_(true);
    this->restart_();
    this->set_timeout(1000, [this]() {
      this->set_config_mode_(true);
      this->query_parameters_();
      this->get_mac_();
      this->get_distance_resolution_();
      this->set_config_mode_(false);
    });
  })
  SUB_LAMBDA_BUTTON(query, {
    this->set_config_mode_(true);
    this->query_parameters_();
    this->get_mac_();
    this->get_distance_resolution_();
    this->get_version_();
    this->set_config_mode_(false);
  })
#endif
#ifdef USE_NUMBER
  SUB_LAMBDA_NUMBER(max_still_distance, { this->set_max_distances_timeout_(); })
  SUB_LAMBDA_NUMBER(max_move_distance, { this->set_max_distances_timeout_(); })
  SUB_LAMBDA_NUMBER(timeout, { this->set_max_distances_timeout_(); })
#endif

 public:
  LD2410Component();
  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_gate_still_threshold_number(int gate, number::Number *n);
  void set_gate_move_threshold_number(int gate, number::Number *n);
  void set_gate_move_sensor(int gate, sensor::Sensor *s);
  void set_gate_still_sensor(int gate, sensor::Sensor *s);
  void set_throttle(uint16_t value) { this->throttle_ = value; };
  void set_bluetooth_password(const std::string &password);
  /// Instantiate a LD2410ComponentCall object to modify this select component's state.
  LD2410ComponentCall make_call() { return LD2410ComponentCall(this); }

 protected:
  int two_byte_to_int_(char firstbyte, char secondbyte) { return (int16_t)(secondbyte << 8) + firstbyte; }
  uint8_t distance_resolution_enum_to_int_(const std::string &distance_resolution) {
    if (distance_resolution == "0.2m") {
      return DISTANCE_RESOLUTION_TWO;
    } else if (distance_resolution == "0.75m") {
      return DISTANCE_RESOLUTION_SEVEN;
    }
    return DISTANCE_RESOLUTION_SEVEN;
  }
  std::string distance_resolution_int_to_enum_(uint8_t distance_resolution) {
    switch (distance_resolution) {
      case DISTANCE_RESOLUTION_TWO:
        return "0.2m";
      case DISTANCE_RESOLUTION_SEVEN:
      default:
        return "0.75m";
    }
  }
  void send_command_(uint8_t command_str, const uint8_t *command_value, int command_value_len);
  void set_max_distances_timeout_();
  void set_gate_threshold_(uint8_t gate);
  void set_config_mode_(bool enable);
  void set_engineering_mode_(bool enable);
  void handle_periodic_data_(uint8_t *buffer, int len);
  bool handle_ack_data_(uint8_t *buffer, int len);
  void readline_(int readch, uint8_t *buffer, int len);
  void query_parameters_();
  void set_bluetooth_(bool enable);
  void set_distance_resolution_(const std::string &state);
  void get_version_();
  void get_mac_();
  void get_distance_resolution_();
  void factory_reset_();
  void restart_();

  int32_t last_periodic_millis_ = millis();
  int32_t last_engineering_mode_change_millis_ = millis();
  uint16_t throttle_;
  std::string version_;
  std::string mac_;
  std::vector<uint8_t> rx_buffer_;
  std::vector<number::Number *> gate_still_threshold_numbers_;
  std::vector<number::Number *> gate_move_threshold_numbers_;
  std::vector<sensor::Sensor *> gate_still_sensors_;
  std::vector<sensor::Sensor *> gate_move_sensors_;
};

}  // namespace ld2410
}  // namespace esphome
