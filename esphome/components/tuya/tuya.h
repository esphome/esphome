#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace tuya {

enum class TuyaDatapointType : uint8_t {
  RAW = 0x00,      // variable length
  BOOLEAN = 0x01,  // 1 byte (0/1)
  INTEGER = 0x02,  // 4 byte
  STRING = 0x03,   // variable length
  ENUM = 0x04,     // 1 byte
  BITMASK = 0x05,  // 2 bytes
};

struct TuyaDatapoint {
  uint8_t id;
  TuyaDatapointType type;
  union {
    bool value_bool;
    int value_int;
    uint32_t value_uint;
    uint8_t value_enum;
    uint16_t value_bitmask;
  };
};

struct TuyaDatapointListener {
  uint8_t datapoint_id;
  std::function<void(TuyaDatapoint)> on_datapoint;
};

enum class TuyaCommandType : uint8_t {
  HEARTBEAT = 0x00,
  QUERY_PRODUCT = 0x01,
  MCU_CONF = 0x02,
  WIFI_STATE = 0x03,
  WIFI_RESET = 0x04,
  WIFI_SELECT = 0x05,
  SET_DATAPOINT = 0x06,
  STATE = 0x07,
  QUERY_STATE = 0x08,
};

class Tuya : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void register_listener(uint8_t datapoint_id, const std::function<void(TuyaDatapoint)> &func);
  void set_datapoint_value(TuyaDatapoint datapoint);

 protected:
  void handle_char_(uint8_t c);
  void handle_datapoint_(const uint8_t *buffer, size_t len);
  bool validate_message_();

  void handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
  void send_command_(TuyaCommandType command, const uint8_t *buffer, uint16_t len);
  void send_empty_command_(TuyaCommandType command) { this->send_command_(command, nullptr, 0); }

  int gpio_status_ = -1;
  int gpio_reset_ = -1;
  std::vector<TuyaDatapointListener> listeners_;
  std::vector<TuyaDatapoint> datapoints_;
  std::vector<uint8_t> rx_message_;
};

}  // namespace tuya
}  // namespace esphome
