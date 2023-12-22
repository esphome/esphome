#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "mbus.h"
#include "mbus-frame-factory.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus";

void MBus::setup() { this->start_time = millis(); }

void MBus::loop() {
  uint32_t now = millis();
  if (now - this->start_time > 1000) {
    scan_primary_addresses();
  }
}

void MBus::dump_config() {
  ESP_LOGCONFIG(TAG, "MBus:");
  // LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  // ESP_LOGCONFIG(TAG, "  Send Wait Time: %d ms", this->send_wait_time_);
  // ESP_LOGCONFIG(TAG, "  CRC Disabled: %s", YESNO(this->disable_crc_));
}

float MBus::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}

void MBus::scan_primary_addresses() {
  static uint8_t address = 0;
  static bool primary_scan_finished = false;

  if (primary_scan_finished) {
    return;
  }

  if (address > 250) {
    primary_scan_finished = true;
    return;
  }

  if (scan_primary_address(address) != 0) {
    address++;
    return;
  }
}

uint8_t MBus::scan_primary_address(uint8_t primary_address) {
  if (!this->_waiting_for_response) {
    ESP_LOGD(TAG, "ping primary address = %d", primary_address);

    // MBus spec states to send REQ_UD2 but sending init-frame is much faster
    // see Chapter 7.3 Searching for Installed Slaves -> Primary Addresses
    MBusFrame frame = MBusFrameFactory::CreateNKEFrame(primary_address);
    send(frame);

    this->_waiting_for_response = true;
    return 0;
  }

  const uint32_t now = millis();
  if ((now - this->last_send_time) > MBUS_RX_TIMEOUT) {
    ESP_LOGV(TAG, "scan_primary_address() rx timeout.");

    this->rx_buffer.clear();
    this->_waiting_for_response = false;

    return -1;
  }

  if (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    ESP_LOGV(TAG, "scan_primary_address(): received data = 0x%X.", byte);

    if (byte == MBUS_FRAME_ACK_START) {
      ESP_LOGI(TAG, "Device found at primary address = %d", primary_address);
    }

    this->rx_buffer.clear();
    this->_waiting_for_response = false;
    return 1;
  }

  return 0;
}

uint8_t MBus::send(MBusFrame &frame) {
  uint8_t payload_size = 0;
  switch (frame.frame_type) {
    case MBUS_FRAME_TYPE_ACK:
      payload_size = MBusFrameBaseSize::ACK_SIZE;
      break;
    case MBUS_FRAME_TYPE_SHORT:
      payload_size = MBusFrameBaseSize::SHORT_SIZE;
      break;
    case MBUS_FRAME_TYPE_CONTROL:
      payload_size = MBusFrameBaseSize::CONTROL_SIZE;
      break;
    case MBUS_FRAME_TYPE_LONG:
      payload_size = MBusFrameBaseSize::LONG_SIZE;
      break;
  }

  std::vector<uint8_t> payload(MBusFrameBaseSize::SHORT_SIZE, 0);
  MBusFrame::serialize(frame, payload);

  ESP_LOGV(TAG, "MBus write: %s", format_hex_pretty(payload).c_str());
  this->last_send_time = millis();

  this->write_array(payload);
  this->flush();

  payload.clear();

  return 0;
}

}  // namespace mbus
}  // namespace esphome
