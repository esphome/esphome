#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
// #include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410 {

#define CHECK_BIT(var, pos) (((var) >> (pos)) & 1)

class LD2410Component : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void loop() override;

  // sensor::Sensor *movingTargetEnergy = new Sensor();
  // sensor::Sensor *stillTargetDistance = new Sensor();
  // sensor::Sensor *stillTargetEnergy = new Sensor();

  // sensor::Sensor *detectDistance = new Sensor();
  void settargetsensor(binary_sensor::BinarySensor *sens) { target_binary_sensor_ = sens; };
  void setmovingtargetsensor(binary_sensor::BinarySensor *sens) { moving_binary_sensor_ = sens; };
  void setstilltargetsensor(binary_sensor::BinarySensor *sens) { still_binary_sensor_ = sens; };
  void setmovingdistancesensor(sensor::Sensor *sens) { moving_target_distance_sensor_ = sens; };
  void setstilldistancesensor(sensor::Sensor *sens) { still_target_distance_sensor_ = sens; };
  void setmovingenergysensor(sensor::Sensor *sens) { moving_target_energy_sensor_ = sens; };
  void setstillenergysensor(sensor::Sensor *sens) { still_target_energy_sensor_ = sens; };
  void setdetectiondistancesensor(sensor::Sensor *sens) { detection_distance_sensor_ = sens; };

  // Number *maxMovingDistanceRange;
  // Number *maxStillDistanceRange;
  int movingSensitivities[9] = {0};
  int stillSensitivities[9] = {0};
  // Number *noneDuration;

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
  // void setNumbers();
  void sendCommand(char *commandStr, char *commandValue, int commandValueLen);
  int twoByteToInt(char firstByte, char secondByte) { return (int16_t)(secondByte << 8) + firstByte; }
  void handlePeriodicData(char *buffer, int len);
  void handleACKData(char *buffer, int len);
  void readline(int readch, char *buffer, int len);
  void queryParameters();
};
}  // namespace ld2410
}  // namespace esphome
