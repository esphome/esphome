#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ld2410 {

#define CHECK_BIT(var, pos) (((var) >> (pos)) & 1)

// Commands
static const uint8_t CMD_ENABLE_CONF = 0x00FF;
static const uint8_t CMD_DISABLE_CONF = 0x00FE;
static const uint8_t CMD_MAXDIST_DURATION = 0x0060;
static const uint8_t CMD_QUERY = 0x0061;
static const uint8_t CMD_GATE_SENS = 0x0064;
static const uint8_t CMD_VERSION = 0x00A0;

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
  DATA_TYPES = 5,
  TARGET_STATES = 8,
  MOVING_TARGET_LOW = 9,
  MOVING_TARGET_HIGH = 10,
  MOVING_ENERGY = 11,
  STILL_TARGET_LOW = 12,
  STILL_TARGET_HIGH = 13,
  STILL_ENERGY = 14,
  DETECT_DISTANCE_LOW = 15,
  DETECT_DISTANCE_HIGH = 16,
};
enum PeriodicDataValue : uint8_t { HEAD = 0XAA, END = 0x55, CHECK = 0x00 };

enum AckDataStructure : uint8_t { COMMAND = 6, COMMAND_STATUS = 7 };

//  char cmd[2] = {enable ? 0xFF : 0xFE, 0x00};
class LD2410Component : public Component, public uart::UARTDevice {
#ifdef USE_SENSOR
  SUB_SENSOR(moving_target_distance)
  SUB_SENSOR(still_target_distance)
  SUB_SENSOR(moving_target_energy)
  SUB_SENSOR(still_target_energy)
  SUB_SENSOR(detection_distance)
#endif

 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

#ifdef USE_BINARY_SENSOR
  void set_target_sensor(binary_sensor::BinarySensor *sens) { this->target_binary_sensor_ = sens; };
  void set_moving_target_sensor(binary_sensor::BinarySensor *sens) { this->moving_binary_sensor_ = sens; };
  void set_still_target_sensor(binary_sensor::BinarySensor *sens) { this->still_binary_sensor_ = sens; };
#endif

  void set_timeout(uint16_t value) { this->timeout_ = value; };
  void set_max_move_distance(uint8_t value) { this->max_move_distance_ = value; };
  void set_max_still_distance(uint8_t value) { this->max_still_distance_ = value; };
  void set_range_config(int rg0_move, int rg0_still, int rg1_move, int rg1_still, int rg2_move, int rg2_still,
                        int rg3_move, int rg3_still, int rg4_move, int rg4_still, int rg5_move, int rg5_still,
                        int rg6_move, int rg6_still, int rg7_move, int rg7_still, int rg8_move, int rg8_still) {
    this->rg0_move_threshold_ = rg0_move;
    this->rg0_still_threshold_ = rg0_still;
    this->rg1_move_threshold_ = rg1_move;
    this->rg1_still_threshold_ = rg1_still;
    this->rg2_move_threshold_ = rg2_move;
    this->rg2_still_threshold_ = rg2_still;
    this->rg3_move_threshold_ = rg3_move;
    this->rg3_still_threshold_ = rg3_still;
    this->rg4_move_threshold_ = rg4_move;
    this->rg4_still_threshold_ = rg4_still;
    this->rg5_move_threshold_ = rg5_move;
    this->rg5_still_threshold_ = rg5_still;
    this->rg6_move_threshold_ = rg6_move;
    this->rg6_still_threshold_ = rg6_still;
    this->rg7_move_threshold_ = rg7_move;
    this->rg7_still_threshold_ = rg7_still;
    this->rg8_move_threshold_ = rg8_move;
    this->rg8_still_threshold_ = rg8_still;
  };
  int moving_sensitivities[9] = {0};
  int still_sensitivities[9] = {0};

  int32_t last_periodic_millis = millis();

 protected:
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *target_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *moving_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *still_binary_sensor_{nullptr};
#endif

  std::vector<uint8_t> rx_buffer_;
  int two_byte_to_int_(char firstbyte, char secondbyte) { return (int16_t) (secondbyte << 8) + firstbyte; }
  void send_command_(uint8_t command_str, uint8_t *command_value, int command_value_len);

  void set_max_distances_timeout_(uint8_t max_moving_distance_range, uint8_t max_still_distance_range,
                                  uint16_t timeout);
  void set_gate_threshold_(uint8_t gate, uint8_t motionsens, uint8_t stillsens);
  void set_config_mode_(bool enable);
  void handle_periodic_data_(uint8_t *buffer, int len);
  void handle_ack_data_(uint8_t *buffer, int len);
  void readline_(int readch, uint8_t *buffer, int len);
  void query_parameters_();
  void get_version_();

  uint16_t timeout_;
  uint8_t max_move_distance_;
  uint8_t max_still_distance_;

  uint8_t version_[6];
  uint8_t rg0_move_threshold_, rg0_still_threshold_, rg1_move_threshold_, rg1_still_threshold_, rg2_move_threshold_,
      rg2_still_threshold_, rg3_move_threshold_, rg3_still_threshold_, rg4_move_threshold_, rg4_still_threshold_,
      rg5_move_threshold_, rg5_still_threshold_, rg6_move_threshold_, rg6_still_threshold_, rg7_move_threshold_,
      rg7_still_threshold_, rg8_move_threshold_, rg8_still_threshold_;
};

}  // namespace ld2410
}  // namespace esphome
