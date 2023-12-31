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

void MBus::setup() { start_scan_primary_addresses(); }

void MBus::loop() { this->_protocol_handler->loop(); }

void MBus::start_scan_primary_addresses() {
  ESP_LOGV(TAG, "start_scan_primary_addresses: 0");

  // MBus spec states to send REQ_UD2 but sending init-frame is much faster
  // see Chapter 7.3 Searching for Installed Slaves -> Primary Addresses
  auto scan_primary_command = MBusFrameFactory::create_nke_frame(0);
  this->_protocol_handler->register_command(*scan_primary_command, scan_primary_addresses_response_handler, 0, 1000);
}

void MBus::scan_primary_addresses_response_handler(MBusCommand *command, const MBusFrame &response) {
  auto address = command->data;
  if (response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_ACK) {
    ESP_LOGD(TAG, "Found mbus slave with primary address = %d.", address);
  }

  address++;
  if (address > PRIMARY_ADDRESS_MAX) {
    start_scan_secondary_addresses(command);
    return;
  }

  ESP_LOGV(TAG, "scan_primary_addresses: %d", address);
  auto scan_primary_command = MBusFrameFactory::create_nke_frame(address);
  command->protocol_handler->register_command(*scan_primary_command, scan_primary_addresses_response_handler, address);
}

void MBus::start_scan_secondary_addresses(MBusCommand *command) {
  ESP_LOGV(TAG, "start_scan_secondary_addresses");

  // init slaves command
  auto init_slaves_command = MBusFrameFactory::create_nke_frame(MBusAddresses::NETWORK_LAYER);
  command->protocol_handler->register_command(*init_slaves_command, scan_secondary_addresses_response_handler, 0, 0,
                                              false);
}

void MBus::scan_secondary_addresses_response_handler(MBusCommand *command, const MBusFrame &response) {
  ESP_LOGV(TAG, "scan_secondary_addresses_response_handler. data = %d", command->data);

  // init slaves command response
  if (command->data == 0) {
    ESP_LOGV(TAG, "send slave select command:");
    std::vector<uint8_t> mask{0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF};
    auto slave_select_command = MBusFrameFactory::create_slave_select(mask);
    command->protocol_handler->register_command(*slave_select_command, scan_secondary_addresses_response_handler, 1);
    return;
  }

  if (command->data == 1 && response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_ACK) {
    ESP_LOGV(TAG, "send REQ_UD2 command:");
    auto req_ud2_command = MBusFrameFactory::create_req_ud2_frame();
    command->protocol_handler->register_command(*req_ud2_command, scan_secondary_addresses_response_handler, 2);
    return;
  }

  if (command->data == 2) {
  }
}

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
