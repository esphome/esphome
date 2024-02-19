
#include "ebus_sensor.h"

// TODO: remove
#define GET_BYTE(CMD, I) ((uint8_t) ((CMD >> 8 * I) & 0XFF))

namespace esphome {
namespace ebus {

void EbusSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "EbusSensor");
  ESP_LOGCONFIG(TAG, "  message:");
  if (this->source_ == SYN) {
    ESP_LOGCONFIG(TAG, "    source: N/A");
  } else {
    ESP_LOGCONFIG(TAG, "    source: 0x%02x", this->source_);
  }
  if (this->destination_ == SYN) {
    ESP_LOGCONFIG(TAG, "    destination: N/A");
  } else {
    ESP_LOGCONFIG(TAG, "    destination: 0x%02x", this->destination_);
  }
  ESP_LOGCONFIG(TAG, "    command: 0x%04x", this->command_);
};

void EbusSensor::set_primary_address(uint8_t primary_address) {
  this->primary_address_ = primary_address;
}
void EbusSensor::set_source(uint8_t source) {
  this->source_ = source;
}
void EbusSensor::set_destination(uint8_t destination) {
  this->destination_ = destination;
}
void EbusSensor::set_command(uint16_t command) {
  this->command_ = command;
}
void EbusSensor::set_payload(const std::vector<uint8_t> &payload) {
  this->payload_ = payload;
}
void EbusSensor::set_response_read_position(uint8_t response_position) {
  this->response_position_ = response_position;
}
void EbusSensor::set_response_read_bytes(uint8_t response_bytes) {
  this->response_bytes_ = response_bytes;
}
void EbusSensor::set_response_read_divider(float response_divider) {
  this->response_divider_ = response_divider;
}

optional<SendCommand> EbusSensor::prepare_command() {
  optional<SendCommand> command;
  if (this->destination_ != SYN) {
    command = SendCommand(  //
         this->primary_address_,
         Elf::to_secondary(this->destination_),
         GET_BYTE(this->command_, 1),
         GET_BYTE(this->command_, 0),
         this->payload_.size(),
         &this->payload_[0]);
  }
  return command;
}

void EbusSensor::process_received(Telegram telegram) {
  if (!is_mine(telegram)) {
    return;
  }
  this->publish_state(to_float(telegram, this->response_position_, this->response_bytes_, this->response_divider_));
}

uint32_t EbusSensor::get_response_bytes(Telegram &telegram, uint8_t start, uint8_t length) {
  uint32_t result = 0;
  for (uint8_t i = 0; i < 4 && i < length; i++) {
    result = result | (telegram.get_response_byte(start + i) << (i * 8));
  }
  return result;
}

float EbusSensor::to_float(Telegram &telegram, uint8_t start, uint8_t length, float divider) {
  return get_response_bytes(telegram, start, length) / divider;
}

bool EbusSensor::is_mine(Telegram &telegram) {
  if (this->source_ != SYN && this->source_ != telegram.getZZ()) {
    return false;
  }
  if (telegram.getCommand() != this->command_) {
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
