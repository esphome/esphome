#include "esphome/core/log.h"

#include "mbus.h"
#include "mbus-frame-factory.h"
#include "mbus-frame-meta.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus";
void MBus::dump_config() {
  ESP_LOGCONFIG(TAG, "MBus:");
  // LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  // ESP_LOGCONFIG(TAG, "  Send Wait Time: %d ms", this->send_wait_time_);
  // ESP_LOGCONFIG(TAG, "  CRC Disabled: %s", YESNO(this->disable_crc_));
}

float MBus::get_setup_priority() const {
  // After UART bus
  return setup_priority::LATE;
}

void MBus::setup() { scan_primary_addresses(); }

void MBus::loop() { this->_protocol_handler->loop(); }

void MBus::scan_primary_addresses() {
  static bool primary_scan_finished = false;
  if (primary_scan_finished) {
    return;
  }

  ESP_LOGV(TAG, "scan_primary_addresses: 0");
  primary_scan_finished = true;

  // MBus spec states to send REQ_UD2 but sending init-frame is much faster
  // see Chapter 7.3 Searching for Installed Slaves -> Primary Addresses
  auto scan_primary_command = MBusFrameFactory::CreateNKEFrame(0);
  this->_protocol_handler->register_command(*scan_primary_command, scan_primary_addresses_response_handler, 0, 1000);
}

void MBus::scan_primary_addresses_response_handler(MBusCommand *command, const MBusFrame &response) {
  auto address = command->data;
  if (response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_ACK) {
    ESP_LOGD(TAG, "Found mbus slave with primary address = %d.", address);
  }

  address++;
  if (address > PRIMARY_ADDRESS_MAX) {
    return;
  }

  ESP_LOGV(TAG, "scan_primary_addresses: %d", address);
  auto scan_primary_command = MBusFrameFactory::CreateNKEFrame(address);
  command->protocol_handler->register_command(*scan_primary_command, scan_primary_addresses_response_handler, address);
}

// int8_t MBus::scan_primary_address(uint8_t primary_address) {
//   static uint8_t scan_state = 0;  // 0 = init, 1 = wait for response
//   if (scan_state == 0) {
//     ESP_LOGD(TAG, "Ping primary address = %d.", primary_address);

//     // MBus spec states to send REQ_UD2 but sending init-frame is much faster
//     // see Chapter 7.3 Searching for Installed Slaves -> Primary Addresses
//     auto frame = MBusFrameFactory::CreateNKEFrame(primary_address);
//     this->_protocol_handler->send(*frame);
//     scan_state = 1;
//     return 0;
//   }

//   if (scan_state == 1) {
//     auto rx_status = this->_protocol_handler->receive();
//     ESP_LOGVV(TAG, "Ping primary address= %d. Received=%d", primary_address, rx_status);

//     // received complete frame
//     if (rx_status == 1 && this->_protocol_handler->is_ack_resonse()) {
//       ESP_LOGI(TAG, "Device found at primary address = %d", primary_address);
//       scan_state = 0;
//       return 1;
//     }

//     // timed out
//     if (rx_status == -1) {
//       scan_state = 0;
//       return -1;
//     }
//   }

//   return 0;
// }

// void MBus::scan_secondary_adresses() {
//   init_slaves();
//   scan_slaves();
// }

// int8_t MBus::init_slaves() {
//   ESP_LOGD(TAG, "Init Slaves.");

//   static uint8_t state = 0;  // 0 = init, 1 = wait for response
//   if (state == 0) {
//     ESP_LOGD(TAG, "Init Slaves.");

//     auto frame = MBusFrameFactory::CreateNKEFrame(MBusAddresses::NETWORK_LAYER);
//     this->_protocol_handler->send(*frame);
//     state = 1;
//     return 0;
//   }

//   if (state == 1) {
//     auto rx_status = this->_protocol_handler->receive();
//     ESP_LOGVV(TAG, "Init slaves. Received=%d", rx_status);

//     // received complete frame
//     if (rx_status == 1 && this->_protocol_handler->is_ack_resonse()) {
//       ESP_LOGD(TAG, "Init slaves completed");
//       state = 0;
//       return 1;
//     }

//     // timed out
//     if (rx_status == -1) {
//       state = 0;
//       return -1;
//     }
//   }

//   return 0;
// }

// int8_t MBus::scan_slaves() {
//   static uint8_t state = 0;  // 0 = init, 1 = wait for response
//   if (state == 0) {
//     ESP_LOGD(TAG, "Scan Slaves.");
//     std::vector<uint8_t> mask{0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF};

//     auto frame = MBusFrameFactory::CreateSlaveSelect(mask);
//     this->_protocol_handler->send(*frame);
//     state = 1;
//     return 0;
//   }

//   if (state == 1) {
//     auto rx_status = this->_protocol_handler->receive();
//     ESP_LOGVV(TAG, "Scan slaves. Received=%d", rx_status);

//     // received complete frame
//     if (rx_status == 1 && this->_protocol_handler->is_ack_resonse()) {
//       ESP_LOGD(TAG, "Scan slaves completed");
//       state = 0;
//       return 1;
//     }

//     // timed out
//     if (rx_status == -1) {
//       state = 0;
//       return -1;
//     }
//   }

//   return 0;
// }

}  // namespace mbus
}  // namespace esphome
