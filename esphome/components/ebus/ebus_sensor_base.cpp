
#include "ebus_sensor_base.h"

namespace esphome {
namespace ebus {

void EbusSensorBase::dump_config() {
  ESP_LOGCONFIG(TAG, "EbusSensor");
  ESP_LOGCONFIG(TAG, "  message:");
  ESP_LOGCONFIG(TAG, "    send_poll: %s", this->send_poll_ ? "true" : "false");
  if (this->address_ == SYN) {
    ESP_LOGCONFIG(TAG, "    address: N/A");
  } else {
    ESP_LOGCONFIG(TAG, "    address: 0x%02x", this->address_);
  }
  ESP_LOGCONFIG(TAG, "    command: 0x%04x", this->command_);
};

void EbusSensorBase::set_send_poll(bool send_poll) { this->send_poll_ = send_poll; }
void EbusSensorBase::set_command(uint16_t command) { this->command_ = command; }
void EbusSensorBase::set_payload(const std::vector<uint8_t> &payload) { this->payload_ = payload; }
void EbusSensorBase::set_response_read_position(uint8_t response_position) { this->response_position_ = response_position; }

optional<SendCommand> EbusSensorBase::prepare_command() {
  optional<SendCommand> command;

  if (this->send_poll_) {
    command = SendCommand(  //
        this->primary_address_, this->address_, this->command_,
        this->payload_.size(), &this->payload_[0]);
  }
  return command;
}

uint32_t EbusSensorBase::get_response_bytes(Telegram &telegram, uint8_t start, uint8_t length) {
  uint32_t result = 0;
  for (uint8_t i = 0; i < 4 && i < length; i++) {
    result = result | (telegram.get_response_byte(start + i) << (i * 8));
  }
  return result;
}

bool EbusSensorBase::is_mine(Telegram &telegram) {
  if (this->address_ != SYN && this->address_ != telegram.get_zz()) {
    return false;
  }
  if (telegram.get_command() != this->command_) {
    return false;
  }
  for (int i = 0; i < this->payload_.size(); i++) {
    if (this->payload_[i] != telegram.get_request_byte(i)) {
      return false;
    }
  }
  return true;
}

}  // namespace ebus
}  // namespace esphome
