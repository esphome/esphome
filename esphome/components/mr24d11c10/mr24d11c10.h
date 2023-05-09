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

#include "radar.h"

namespace esphome {
namespace mr24d11c10 {

// Header
#define MESSAGE_HEAD 0x55

// Funtions
#define READ_CONFIG 0x01
#define WRITE_CONFIG 0x02
#define PASSIVE_REPORT 0x03
#define ACTIVE_REPORT 0x04

// Address functions 1
#define REPORT_RADAR 0x03
#define REPORT_OTHER 0x05

// Address functions 2
#define HEARTBEAT 0x01
#define ABNOEMAL 0x02
#define ENVIRONMENT 0x05
#define BODYSIGN 0x06
#define CLOSE_AWAY 0x07

// Movement options
#define CA_BE 0x01
#define CA_CLOSE 0x02
#define CA_AWAY 0x03
#define SOMEBODY_BE 0x01
#define SOMEBODY_MOVE 0x01
#define SOMEBODY_STOP 0x00
#define NOBODY 0x00

// Radar informations
#define DEVICE_ID 0x01
#define SW_VERSION 0x02
#define HW_VERSION 0x03
#define PROTOCOL_VERSION 0x04
#define SCENE 0x10
#define DATA_OFFSET 4

class MR24D11C10Component : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void setup() override;
  void loop() override;

  SeeedStudio_Radar(UARTComponent *parent) : UARTDevice(parent) {}
  radar *seeedRadar;
  uint8_t buffer[64];
  size_t msg_len;


  Sensor *body_movement_ = new Sensor();
  BinarySensor *target_present_ = new BinarySensor();

  void set_human_presence_binary_sensor(binary_sensor::BinarySensor *sens) { this->target_present_ = sens; };
  void set_body_movement_sensor(Sensor::Sensor *sens) { this->body_movement_ = sens; };

 protected:
  binary_sensor::BinarySensor *target_present_{nullptr};
  sensor::Sensor *body_movement_{nullptr};
};

}  // namespace mr24d11c10
}  // namespace esphome
