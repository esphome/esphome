#include "duco.h"
#include "text_sensor/sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <vector>

namespace esphome {
namespace duco {

static const char *const TAG = "duco";

void Duco::setup() {
  // no setup needed
}
void Duco::loop() {
  const uint32_t now = millis();

  if (now - this->last_byte_ > 50) {
    // first try to finalize last message, before clearing the buffer
    this->finalize_message_();

    this->rx_buffer_.clear();
    this->last_byte_ = now;
  }

  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    if (this->parse_byte_(byte)) {
      this->last_byte_ = now;
    } else {
      this->rx_buffer_.clear();
    }
  }
}

bool Duco::parse_byte_(uint8_t byte) {
  size_t at = this->rx_buffer_.size();
  this->rx_buffer_.push_back(byte);
  const uint8_t *raw = &this->rx_buffer_[0];
  ESP_LOGV(TAG, "DUCO received Byte  %d (0X%x)", byte, byte);
  // Byte 0: Packet length (match all)
  if (at == 0)
    return true;

  // Byte 1: check if AA:55 header
  if (at == 1 && raw[0] == 0xAA && raw[1] == 0x55) {
    // ignore the AA:55 header
    return false;
  }

  // Byte 1: function code
  if (at == 1) {
    return true;
  }

  if (at >= 2 && this->rx_buffer_[at - 1] == 0xAA && this->rx_buffer_[at] == 0x01 && !this->removed_last_) {
    // The last byte was 0xAA, but the current one is 0x01, which means we ignore it (only once!)
    // This is an edge case in processing, likely to prevent accidentally introducing AA:55 in a message
    this->rx_buffer_.pop_back();
    this->removed_last_ = true;
    return true;
  }
  this->removed_last_ = false;

  // Byte 2: Identification code
  if (at == 2)
    return true;

  // finalize message when AA:55 is found
  if (at >= 2 && this->rx_buffer_[at - 1] == 0xAA && this->rx_buffer_[at] == 0x55) {
    this->rx_buffer_.pop_back();
    this->rx_buffer_.pop_back();
    at--;
    at--;

    this->finalize_message_();

    // reset buffer after finalizing
    return false;
  }

  return true;
}

void Duco::finalize_message_() {
  if (this->rx_buffer_.size() == 0)
    return;
  const uint8_t *raw = &this->rx_buffer_[0];

  uint8_t data_len = raw[0];

  // Byte data_offset+len+1: CRC_HI (over all bytes)
  uint16_t computed_crc = crc16(raw, data_len + 1);
  uint16_t remote_crc = uint16_t(raw[data_len + 1]) | (uint16_t(raw[data_len + 2]) << 8);
  if (computed_crc != remote_crc) {
    if (this->disable_crc_) {
      ESP_LOGD(TAG, "CRC Check failed, but ignored! %02X!=%02X", computed_crc, remote_crc);
    } else {
      ESP_LOGW(TAG, "CRC Check failed! %02X!=%02X", computed_crc, remote_crc);
      return;
    }
  }
  std::vector<uint8_t> data(this->rx_buffer_.begin(), this->rx_buffer_.begin() + data_len + 1);

  // see if the response exists for waiting
  auto it = waiting_for_response.find(data[2]);
  if (it != waiting_for_response.end()) {
    waiting_for_response[data[2]]->receive_response(data);
  }
}

void Duco::dump_config() {
  ESP_LOGCONFIG(TAG, "DUCO:");
  ESP_LOGCONFIG(TAG, "  Send Wait Time: %d ms", this->send_wait_time_);
  ESP_LOGCONFIG(TAG, "  CRC Disabled: %s", YESNO(this->disable_crc_));
}

float Duco::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}

uint8_t Duco::next_id_() {
  last_id_++;
  if (last_id_ == 0xAA or last_id_ == 0x55) {
    return next_id_();
  }
  return last_id_;
}

void Duco::stop_waiting(uint8_t message_id) {
  auto it = waiting_for_response.find(message_id);

  if (it != waiting_for_response.end()) {
    waiting_for_response.erase(it);
  }
}

void Duco::send(uint8_t function, std::vector<uint8_t> message, DucoDevice *device) {
  std::vector<uint8_t> data;
  uint8_t message_id = this->next_id_();

  data.push_back(message.size() + 2);
  data.push_back(function);
  data.push_back(message_id);
  data.insert(data.end(), message.begin(), message.end());

  send_raw(data);

  this->waiting_for_response[message_id] = device;
}

// Helper function for lambdas
// Send raw command. Except CRC everything must be contained in payload
void Duco::send_raw(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    return;
  }

  auto crc = crc16(payload.data(), payload.size());

  this->write_byte(0xAA);
  this->write_byte(0x55);
  this->write_array(payload);
  this->write_byte(crc & 0xFF);
  this->write_byte((crc >> 8) & 0xFF);
  this->flush();

  ESP_LOGD(TAG, "DUCO write: %s", format_hex_pretty(payload).c_str());
  last_send_ = millis();
}

void Duco::debug_hex(std::vector<uint8_t> bytes, uint8_t separator) {
  std::string res;
  res += "DUCO msg: ";

  size_t len = bytes.size();

  char buf[5];

  for (size_t i = 0; i < len; i++) {
    if (i > 0) {
      res += separator;
    }
    sprintf(buf, "%02X", bytes[i]);
    res += buf;
  }
  ESP_LOGD(TAG, "%s", res.c_str());
  delay(10);
}

void Duco::add_sensor_item(DucoDevice *sensor) { sensor->set_parent(this); }

}  // namespace duco
}  // namespace esphome
