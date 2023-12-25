#include "esphome/core/log.h"

#include "mbus.h"
#include "mbus-frame-factory.h"
#include "mbus-frame-meta.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus";

void MBus::setup() { this->_start_time = millis(); }

void MBus::loop() {
  uint32_t now = millis();
  if (now - this->_start_time > 1000) {
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

int8_t MBus::scan_primary_address(uint8_t primary_address) {
  static uint8_t scan_state = 0;  // 0 = init, 1 = wait for response
  if (scan_state == 0) {
    ESP_LOGD(TAG, "Ping primary address = %d.", primary_address);

    // MBus spec states to send REQ_UD2 but sending init-frame is much faster
    // see Chapter 7.3 Searching for Installed Slaves -> Primary Addresses
    MBusFrame frame = MBusFrameFactory::CreateNKEFrame(primary_address);
    this->_protocol_handler->send(frame);
    scan_state = 1;
    return 0;
  }

  if (scan_state == 1) {
    auto rx_status = this->_protocol_handler->receive();
    ESP_LOGVV(TAG, "Ping primary address= %d. Received=%d", primary_address, rx_status);

    // received complete frame
    if (rx_status == 1) {
      ESP_LOGI(TAG, "Device found at primary address = %d", primary_address);
      scan_state = 0;
      return 1;
    }

    // timed out
    if (rx_status == -1) {
      scan_state = 0;
      return -1;
    }
  }

  return 0;
}

}  // namespace mbus
}  // namespace esphome
