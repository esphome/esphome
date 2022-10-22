#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
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

static const uint8_t CMD_FRAME_HEADER[4] = {0xFD, 0xFC, 0xFB, 0xFA};
static const uint8_t CMD_FRAME_END[4] = {0x04, 0x03, 0x02, 0x01};
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
  DETECT_DISTANCE_HIGH = 16
};
enum AckDataStructure : uint8_t { COMMAND = 6, COMMAND_STATUS = 7 };

//  char cmd[2] = {enable ? 0xFF : 0xFE, 0x00};
class LD2410Component : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void loop() override;

  void set_config_mode_(bool enable);
  void set_target_sensor(binary_sensor::BinarySensor *sens) { this->target_binary_sensor_ = sens; };
  void set_moving_target_sensor(binary_sensor::BinarySensor *sens) { this->moving_binary_sensor_ = sens; };
  void set_still_target_sensor(binary_sensor::BinarySensor *sens) { this->still_binary_sensor_ = sens; };
  void set_moving_distance_sensor(sensor::Sensor *sens) { this->moving_target_distance_sensor_ = sens; };
  void set_still_distance_sensor(sensor::Sensor *sens) { this->still_target_distance_sensor_ = sens; };
  void set_moving_energy_sensor(sensor::Sensor *sens) { this->moving_target_energy_sensor_ = sens; };
  void set_still_energy_sensor(sensor::Sensor *sens) { this->still_target_energy_sensor_ = sens; };
  void set_detection_distance_sensor(sensor::Sensor *sens) { this->detection_distance_sensor_ = sens; };
  void set_none_duration(int value) { this->noneDuration_ = value; };
  void set_max_move_distance(int value) { this->maxMoveDistance_ = value; };
  void set_max_still_distance(int value) { this->maxStillDistance_ = value; };
  void set_range_config(int rg0_move, int rg0_still, int rg1_move, int rg1_still, int rg2_move, int rg2_still,
                        int rg3_move, int rg3_still, int rg4_move, int rg4_still, int rg5_move, int rg5_still,
                        int rg6_move, int rg6_still, int rg7_move, int rg7_still, int rg8_move, int rg8_still) {
    this->rg0_move_sens_ = rg0_move;
    this->rg0_still_sens_ = rg0_still;
    this->rg1_move_sens_ = rg1_move;
    this->rg1_still_sens_ = rg1_still;
    this->rg2_move_sens_ = rg2_move;
    this->rg2_still_sens_ = rg2_still;
    this->rg3_move_sens_ = rg3_move;
    this->rg3_still_sens_ = rg3_still;
    this->rg4_move_sens_ = rg4_move;
    this->rg4_still_sens_ = rg4_still;
    this->rg5_move_sens_ = rg5_move;
    this->rg5_still_sens_ = rg5_still;
    this->rg6_move_sens_ = rg6_move;
    this->rg6_still_sens_ = rg6_still;
    this->rg7_move_sens_ = rg7_move;
    this->rg7_still_sens_ = rg7_still;
    this->rg8_move_sens_ = rg8_move;
    this->rg8_still_sens_ = rg8_still;
  };
  // Number *maxMovingDistanceRange;
  // Number *maxStillDistanceRange;
  // Number *noneDuration;
  int moving_sensitivities[9] = {0};
  int still_sensitivities[9] = {0};

  long last_periodic_millis = millis();

 protected:
  binary_sensor::BinarySensor *target_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *moving_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *still_binary_sensor_{nullptr};
  sensor::Sensor *moving_target_distance_sensor_{nullptr};
  sensor::Sensor *still_target_distance_sensor_{nullptr};
  sensor::Sensor *moving_target_energy_sensor_{nullptr};
  sensor::Sensor *still_target_energy_sensor_{nullptr};
  sensor::Sensor *detection_distance_sensor_{nullptr};

  int twoByteToInt_(char firstbyte, char secondbyte) { return (int16_t)(secondbyte << 8) + firstbyte; }
  void sendCommand_(uint8_t command_str, char *command_value, int command_value_len);

  void set_max_distances_none_duration_(int max_moving_distance_range, int max_still_distance_range, int none_duration);
  void set_sensitivity_(uint8_t gate, uint8_t motion_sens, uint8_t still_sens);

  void handlePeriodicData_(char *buffer, int len);
  void handleACKData_(char *buffer, int len);
  void readline_(int readch, char *buffer, int len);
  void queryParameters_();
  void getVersion_();
  int noneDuration_ = -1;
  int maxMoveDistance_ = -1;
  int maxStillDistance_ = -1;
  int rg0_move_sens_, rg0_still_sens_, rg1_move_sens_, rg1_still_sens_, rg2_move_sens_, rg2_still_sens_, rg3_move_sens_,
      rg3_still_sens_, rg4_move_sens_, rg4_still_sens_, rg5_move_sens_, rg5_still_sens_, rg6_move_sens_,
      rg6_still_sens_, rg7_move_sens_, rg7_still_sens_, rg8_move_sens_, rg8_still_sens_ = -1;
};

template<typename... Ts> class LD2410SetConfigMode : public Action<Ts...> {
 public:
  LD2410SetConfigMode(LD2410Component *ld2410) : ld2410_(ld2410) {}

  void play(Ts... x) override { this->ld2410_->set_config_mode_(true); }

 protected:
  LD2410Component *ld2410_;
};

}  // namespace ld2410
}  // namespace esphome
