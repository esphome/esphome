#pragma once

#include "../ebus_component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ebus {

class EbusSensor : public EbusReceiver, public EbusSender, public sensor::Sensor, public Component {
 public:
  EbusSensor() {}

  void dump_config() override;

  void set_primary_address(uint8_t /*primary_address*/) override;
  void set_source(uint8_t /*source*/);
  void set_destination(uint8_t /*destination*/);
  void set_command(uint16_t /*command*/);
  void set_payload(const std::vector<uint8_t> & /*payload*/);

  void set_response_read_position(uint8_t /*response_position*/);
  void set_response_read_bytes(uint8_t /*response_bytes*/);
  void set_response_read_divider(float /*response_divider*/);

  void process_received(Telegram /*telegram*/) override;
  optional<SendCommand> prepare_command() override;

  // TODO: refactor these
  uint32_t get_response_bytes(Telegram &telegram, uint8_t start, uint8_t length);
  float to_float(Telegram &telegram, uint8_t start, uint8_t length, float divider);
  bool is_mine(Telegram &telegram);

 protected:
  uint8_t primary_address_;
  uint8_t source_ = SYN;
  uint8_t destination_ = SYN;
  uint16_t command_;
  std::vector<uint8_t> payload_{};
  uint8_t response_position_;
  uint8_t response_bytes_;
  float response_divider_;
};

}  // namespace ebus
}  // namespace esphome
