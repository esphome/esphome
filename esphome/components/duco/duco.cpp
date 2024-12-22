#include "duco.h"
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
  if (this->rx_buffer_.empty())
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

  DucoMessage message;
  message.function = this->rx_buffer_[1];
  message.id = this->rx_buffer_[2];
  message.data.insert(message.data.end(), this->rx_buffer_.begin() + 3, this->rx_buffer_.begin() + data_len + 1);

  // see if a component is waiting for a response
  auto it = waiting_for_response.find(message.id);
  if (it != waiting_for_response.end()) {
    waiting_for_response[message.id]->receive_response(message);
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

void Duco::send(DucoMessage message, DucoDevice *device) {
  message.id = this->next_id_();

  this->write_byte(0xAA);
  this->write_byte(0x55);
  this->write_array(message.get_message());
  this->flush();

  this->waiting_for_response[message.id] = device;
  ESP_LOGD(TAG, "Duco message sent: %s", message.to_string().c_str());
}

void Duco::debug_hex_(std::vector<uint8_t> bytes, uint8_t separator) {
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

const std::string DucoDiscovery::NODE_TYPE_UCBAT = "UCBAT";
const std::string DucoDiscovery::NODE_TYPE_UC = "UC";
const std::string DucoDiscovery::NODE_TYPE_UCCO2 = "UCCO2";
const std::string DucoDiscovery::NODE_TYPE_BOX = "BOX";
const std::string DucoDiscovery::NODE_TYPE_SWITCH = "SWITCH";
const std::string DucoDiscovery::NODE_TYPE_UNKNOWN = "UNKNOWN";

std::string friendly_node_type(uint8_t type_code) {
  switch (type_code) {
    case DucoDiscovery::NODE_TYPE_CODE_UCBAT:
      return DucoDiscovery::NODE_TYPE_UCBAT;
    case DucoDiscovery::NODE_TYPE_CODE_UC:
      return DucoDiscovery::NODE_TYPE_UC;
    case DucoDiscovery::NODE_TYPE_CODE_UCCO2:
      return DucoDiscovery::NODE_TYPE_UCCO2;
    case DucoDiscovery::NODE_TYPE_CODE_BOX:
      return DucoDiscovery::NODE_TYPE_BOX;
    case DucoDiscovery::NODE_TYPE_CODE_SWITCH:
      return DucoDiscovery::NODE_TYPE_SWITCH;
    default:
      return DucoDiscovery::NODE_TYPE_UNKNOWN;
  }
}

void DucoDiscovery::update() {
  // display all found nodes
  ESP_LOGI(TAG, "Discovered nodes:");
  for (auto &node : nodes_) {
    ESP_LOGI(TAG, "  Node %d: type %d (%s)", std::get<0>(node), std::get<1>(node),
             friendly_node_type(std::get<1>(node)).c_str());
  }
}

void DucoDiscovery::loop() {
  if (delay_ > 0) {
    delay_--;
    return;
  }
  if (next_node_ > 0x44) {
    // discovery is finished
    return;
  }
  if (!waiting_for_response_) {
    ESP_LOGD(TAG, "Discover next node (%d = 0x%02x)", next_node_, next_node_);
    // request the next node
    DucoMessage message;
    message.function = 0x0c;
    message.data = {0x01, next_node_};
    this->parent_->send(message, this);

    waiting_for_response_ = true;
  }
}

void DucoDiscovery::receive_response(DucoMessage message) {
  if (message.function == 0x0e) {
    ESP_LOGV(TAG, "Discovery response message: %s", format_hex_pretty(message.get_message()).c_str());
    this->parent_->stop_waiting(message.id);

    if (message.data[0] != 0x00) {
      // node was found, store its information
      nodes_.push_back(std::make_tuple(next_node_, message.data[0]));
    }

    next_node_++;
    waiting_for_response_ = false;
    // delay execution for 100 loops
    delay_ = 100;
  }
}

}  // namespace duco
}  // namespace esphome
