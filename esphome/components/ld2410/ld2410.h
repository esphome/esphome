#pragma once
#include "esphome.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
// #include "esphome/components/number/number.h"

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
enum periodic_data_structure : uint8_t {
  dataTypes = 5,
  targetStates = 8,
  movingTargetLow = 9,
  movingTargetHigh = 10,
  movingEnergy = 11,
  stillTargetLow = 12,
  stillTargetHigh = 13,
  stillEnergy = 14,
  detectDistanceLow = 15,
  detectDistanceHigh = 16
};
enum ack_data_structure : uint8_t { command = 6, command_status = 7 };

//  char cmd[2] = {enable ? 0xFF : 0xFE, 0x00};
class LD2410Component : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void loop() override;

  void setConfigMode(bool enable);
  void settargetsensor(binary_sensor::BinarySensor *sens) { target_binary_sensor_ = sens; };
  void setmovingtargetsensor(binary_sensor::BinarySensor *sens) { moving_binary_sensor_ = sens; };
  void setstilltargetsensor(binary_sensor::BinarySensor *sens) { still_binary_sensor_ = sens; };
  void setmovingdistancesensor(sensor::Sensor *sens) { moving_target_distance_sensor_ = sens; };
  void setstilldistancesensor(sensor::Sensor *sens) { still_target_distance_sensor_ = sens; };
  void setmovingenergysensor(sensor::Sensor *sens) { moving_target_energy_sensor_ = sens; };
  void setstillenergysensor(sensor::Sensor *sens) { still_target_energy_sensor_ = sens; };
  void setdetectiondistancesensor(sensor::Sensor *sens) { detection_distance_sensor_ = sens; };
  void setNoneDuration(int value) { noneDuration_ = value; };
  void setMaxMoveDistance(int value) { maxMoveDistance_ = value; };
  void setMaxStillDistance(int value) { maxStillDistance_ = value; };
  void setRangeConfig(int rg0_move, int rg0_still, int rg1_move, int rg1_still, int rg2_move, int rg2_still,
                      int rg3_move, int rg3_still, int rg4_move, int rg4_still, int rg5_move, int rg5_still,
                      int rg6_move, int rg6_still, int rg7_move, int rg7_still, int rg8_move, int rg8_still) {
    rg0_move_sens_ = rg0_move;
    rg0_still_sens_ = rg0_still;
    rg1_move_sens_ = rg1_move;
    rg1_still_sens_ = rg1_still;
    rg2_move_sens_ = rg2_move;
    rg2_still_sens_ = rg2_still;
    rg3_move_sens_ = rg3_move;
    rg3_still_sens_ = rg3_still;
    rg4_move_sens_ = rg4_move;
    rg4_still_sens_ = rg4_still;
    rg5_move_sens_ = rg5_move;
    rg5_still_sens_ = rg5_still;
    rg6_move_sens_ = rg6_move;
    rg6_still_sens_ = rg6_still;
    rg7_move_sens_ = rg7_move;
    rg7_still_sens_ = rg7_still;
    rg8_move_sens_ = rg8_move;
    rg8_still_sens_ = rg8_still;
  };
  // Number *maxMovingDistanceRange;
  // Number *maxStillDistanceRange;
  // Number *noneDuration;
  int movingSensitivities[9] = {0};
  int stillSensitivities[9] = {0};

  long lastPeriodicMillis = millis();

 private:
  binary_sensor::BinarySensor *target_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *moving_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *still_binary_sensor_{nullptr};
  sensor::Sensor *moving_target_distance_sensor_{nullptr};
  sensor::Sensor *still_target_distance_sensor_{nullptr};
  sensor::Sensor *moving_target_energy_sensor_{nullptr};
  sensor::Sensor *still_target_energy_sensor_{nullptr};
  sensor::Sensor *detection_distance_sensor_{nullptr};
  bool hastargetsensor() { return (target_binary_sensor_ != nullptr); };
  bool hasmovingtargetsensor() { return (moving_binary_sensor_ != nullptr); };
  bool hasstilltargetsensor() { return (still_binary_sensor_ != nullptr); };
  void sendCommand(char *commandStr, char *commandValue, int commandValueLen);
  int twoByteToInt(char firstByte, char secondByte) { return (int16_t)(secondByte << 8) + firstByte; }
  void sendCommand(uint8_t commandStr, char *commandValue, int commandValueLen);

  void setMaxDistancesAndNoneDuration(int maxMovingDistanceRange, int maxStillDistanceRange, int noneDuration);
  void setSensitivity(uint8_t gate, uint8_t motionsens, uint8_t stillsens);

  void handlePeriodicData(char *buffer, int len);
  void handleACKData(char *buffer, int len);
  void readline(int readch, char *buffer, int len);
  void queryParameters();
  void getVersion();
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

  void play(Ts... x) override { this->ld2410_->setConfigMode(true); }

 protected:
  LD2410Component *ld2410_;
};

}  // namespace ld2410
}  // namespace esphome
