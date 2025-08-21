#include "zwave_proxy.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zwave_proxy {

static const char *TAG = "zwave_proxy";

ZWaveProxy::ZWaveProxy() { global_zwave_proxy = this; }

void ZWaveProxy::setup() {
  // Get capabilities command sent once here just to test communication for component development
  uint8_t get_capabilities_cmd[] = {0x01, 0x03, 0x00, 0x07, 0xfb};
  ESP_LOGD(TAG, "Sending: %s", format_hex_pretty(get_capabilities_cmd, sizeof(get_capabilities_cmd)).c_str());
  this->write_array(get_capabilities_cmd, sizeof(get_capabilities_cmd));
}

void ZWaveProxy::loop() {
  if (this->response_handler_()) {
    return;  // If a response was handled, exit early to avoid a CAN
  }

  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      this->status_set_warning("Failed reading from UART");
      return;
    }
    if (this->parse_byte_(byte)) {
      // TODO: send frame to client(s)...
      ESP_LOGD(TAG, "Frame complete");
    }
  }
  this->status_clear_warning();
}

void ZWaveProxy::dump_config() { ESP_LOGCONFIG(TAG, "Z-Wave Proxy"); }

void ZWaveProxy::send_frame(const std::string &data) {
  ESP_LOGD(TAG, "Sending: %s", format_hex_pretty(data).c_str());
  this->write_array((uint8_t *) data.data(), data.size());
}

void ZWaveProxy::send_frame(const std::vector<uint8_t> &data) {
  ESP_LOGD(TAG, "Sending: %s", format_hex_pretty(data).c_str());
  this->write_array(data);
}

bool ZWaveProxy::parse_byte_(uint8_t byte) {
  // Basic parsing logic for received frames
  switch (this->parsing_state_) {
    case ZWAVE_PARSING_STATE_WAIT_START:
      this->parse_start_(byte);
      break;
    case ZWAVE_PARSING_STATE_WAIT_LENGTH:
      ESP_LOGD(TAG, "Received LENGTH: %u", byte);
      this->end_frame_after_ = this->buffer_index_ + byte;
      ESP_LOGVV(TAG, "Calculated EOF: %u", this->end_frame_after_);
      this->buffer_[this->buffer_index_++] = byte;
      this->checksum_ ^= byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_TYPE;
      break;
    case ZWAVE_PARSING_STATE_WAIT_TYPE:
      this->buffer_[this->buffer_index_++] = byte;
      ESP_LOGD(TAG, "Received TYPE: 0x%02X", byte);
      this->checksum_ ^= byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_COMMAND_ID;
      break;
    case ZWAVE_PARSING_STATE_WAIT_COMMAND_ID:
      this->buffer_[this->buffer_index_++] = byte;
      ESP_LOGD(TAG, "Received COMMAND ID: 0x%02X", byte);
      this->checksum_ ^= byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_PAYLOAD;
      break;
    case ZWAVE_PARSING_STATE_WAIT_PAYLOAD:
      this->buffer_[this->buffer_index_++] = byte;
      this->checksum_ ^= byte;
      ESP_LOGVV(TAG, "Received PAYLOAD: 0x%02X", byte);
      if (this->buffer_index_ >= this->end_frame_after_) {
        this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_CHECKSUM;
      }
      break;
    case ZWAVE_PARSING_STATE_WAIT_CHECKSUM:
      this->buffer_[this->buffer_index_++] = byte;
      ESP_LOGD(TAG, "Received CHECKSUM: 0x%02X", byte);
      ESP_LOGV(TAG, "Calculated CHECKSUM: 0x%02X", this->checksum_);
      if (this->checksum_ != byte) {
        ESP_LOGW(TAG, "Bad checksum: expected 0x%02X, got 0x%02X", this->checksum_, byte);
        this->parsing_state_ = ZWAVE_PARSING_STATE_SEND_NAK;
      } else {
        this->parsing_state_ = ZWAVE_PARSING_STATE_SEND_ACK;
        ESP_LOGD(TAG, "Received frame: %s", format_hex_pretty(this->buffer_, this->buffer_index_).c_str());
        return true;
      }
      break;
    case ZWAVE_PARSING_STATE_SEND_ACK:
    case ZWAVE_PARSING_STATE_SEND_NAK:
      break;  // Should not happen, handled in loop()
    default:
      ESP_LOGD(TAG, "Received unknown byte: 0x%02X", byte);
      break;
  }
  return false;
}

void ZWaveProxy::parse_start_(uint8_t byte) {
  this->buffer_index_ = 0;
  this->checksum_ = 0xFF;
  this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_START;
  switch (byte) {
    case ZWAVE_FRAME_TYPE_START:
      ESP_LOGD(TAG, "Received START");
      this->buffer_[this->buffer_index_++] = byte;
      this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_LENGTH;
      break;
    case ZWAVE_FRAME_TYPE_ACK:
      ESP_LOGD(TAG, "Received ACK");
      break;
    case ZWAVE_FRAME_TYPE_NAK:
      ESP_LOGW(TAG, "Received NAK");
      break;
    case ZWAVE_FRAME_TYPE_CAN:
      ESP_LOGW(TAG, "Received CAN");
      break;
    default:
      ESP_LOGW(TAG, "Unexpected type: 0x%02X", byte);
      break;
  }
}

bool ZWaveProxy::response_handler_() {
  uint8_t response_byte = 0;
  switch (this->parsing_state_) {
    case ZWAVE_PARSING_STATE_SEND_ACK:
      response_byte = ZWAVE_FRAME_TYPE_ACK;
      break;
    case ZWAVE_PARSING_STATE_SEND_CAN:
      response_byte = ZWAVE_FRAME_TYPE_CAN;
      break;
    case ZWAVE_PARSING_STATE_SEND_NAK:
      response_byte = ZWAVE_FRAME_TYPE_NAK;
      break;
    default:
      return false;  // No response handled
  }

  ESP_LOGD(TAG, "Sending %s (0x%02X)", response_byte == ZWAVE_FRAME_TYPE_ACK ? "ACK" : "NAK/CAN", response_byte);
  this->write_byte(response_byte);
  this->parsing_state_ = ZWAVE_PARSING_STATE_WAIT_START;
  return true;
}

ZWaveProxy *global_zwave_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace zwave_proxy
}  // namespace esphome
