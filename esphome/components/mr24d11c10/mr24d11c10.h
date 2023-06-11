#pragma once
#include "esphome/core/static const uint8_ts.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include "radar.h"

namespace esphome {
namespace mr24d11c10 {

// Header
static const uint8_t MESSAGE_HEAD = 0x55;

// Funtions
static const uint8_t READ_CONFIG = 0x01;
static const uint8_t WRITE_CONFIG = 0x02;
static const uint8_t PASSIVE_REPORT = 0x03;
static const uint8_t ACTIVE_REPORT = 0x04;

// Address functions 1
static const uint8_t REPORT_RADAR = 0x03;
static const uint8_t REPORT_OTHER = 0x05;

// Address functions 2
static const uint8_t HEARTBEAT = 0x01;
static const uint8_t ABNOEMAL = 0x02;
static const uint8_t ENVIRONMENT = 0x05;
static const uint8_t BODYSIGN = 0x06;
static const uint8_t CLOSE_AWAY = 0x07;

// Movement options
static const uint8_t CA_BE = 0x01;
static const uint8_t CA_CLOSE = 0x02;
static const uint8_t CA_AWAY = 0x03;
static const uint8_t SOMEBODY_BE = 0x01;
static const uint8_t SOMEBODY_MOVE = 0x01;
static const uint8_t SOMEBODY_STOP = 0x00;
static const uint8_t NOBODY = 0x00;

// Radar informations
static const uint8_t DEVICE_ID = 0x01;
static const uint8_t SW_VERSION = 0x02;
static const uint8_t HW_VERSION = 0x03;
static const uint8_t PROTOCOL_VERSION = 0x04;
static const uint8_t SCENE = 0x10;
static const uint8_t DATA_OFFSET = 4;

class MR24D11C10Component : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void setup() override;
  void loop() override;

  radar_ns::radar *seeedRadar;
  uint8_t buffer[64];
  size_t msg_len;

  void set_human_presence_binary_sensor(binary_sensor::BinarySensor *sens) { this->target_present_ = sens; };
  void set_body_movement_sensor(sensor::Sensor *sens) { this->body_movement_ = sens; };
  void send_new_scene_settings(uint8_t id);
  void send_new_threshold(uint8_t id);
  void get_radar_device_id();
  void printBufferOnLine();
  void active_result();
  void read_configs();
  void process_message();
  void send_command(uint8_t *buff, uint8_t data_length);

 protected:
  binary_sensor::BinarySensor *target_present_{nullptr};
  sensor::Sensor *body_movement_{nullptr};
};

}  // namespace mr24d11c10
}  // namespace esphome
