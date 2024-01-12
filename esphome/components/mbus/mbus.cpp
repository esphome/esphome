#include "esphome/core/log.h"

#include "mbus.h"
#include "mbus-frame-factory.h"
#include "mbus-frame-meta.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus";

void MBus::dump_config() {
  ESP_LOGCONFIG(TAG, "MBus:");
  ESP_LOGCONFIG(TAG, "  Secondary Address: 0x%.016llX", this->secondary_address);
}

float MBus::get_setup_priority() const {
  // After UART bus
  return setup_priority::LATE;
}

void MBus::setup() {
  if (this->secondary_address == 0) {
    start_scan_primary_addresses(this);
  }
}

void MBus::loop() { this->_protocol_handler->loop(); }

void MBus::set_secondary_address(uint64_t secondary_address) { this->secondary_address = secondary_address; }

void MBus::start_scan_primary_addresses(MBus *mbus) {
  ESP_LOGV(TAG, "start_scan_primary_addresses: %.2X", mbus->primary_address);

  // MBus spec states to send REQ_UD2 but sending init-frame is much faster
  // see Chapter 7.3 Searching for Installed Slaves -> Primary Addresses
  auto scan_primary_command = MBusFrameFactory::create_nke_frame(mbus->primary_address);
  mbus->_protocol_handler->register_command(*scan_primary_command, scan_primary_addresses_response_handler,
                                            /*step*/ 0, /*delay*/ 1000);
}

void MBus::scan_primary_addresses_response_handler(MBusCommand *command, const MBusFrame &response) {
  auto mbus = command->mbus;
  if (command->step == 0 && response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_ACK) {
    ESP_LOGD(TAG, "Found mbus slave with primary address = %.2X.", mbus->primary_address);
    start_scan_secondary_addresses(command->mbus);
    return;
  }

  mbus->primary_address++;
  if (mbus->primary_address > PRIMARY_ADDRESS_MAX) {
    ESP_LOGE(TAG, "No Device with primary address found.");
    return;
  }

  ESP_LOGV(TAG, "scan_primary_addresses: %.2X", mbus->primary_address);
  auto scan_primary_command = MBusFrameFactory::create_nke_frame(mbus->primary_address);
  mbus->_protocol_handler->register_command(*scan_primary_command, scan_primary_addresses_response_handler,
                                            /*step*/ 0, /*delay*/ 1000);
}

void MBus::start_scan_secondary_addresses(MBus *mbus) {
  ESP_LOGV(TAG, "start_scan_secondary_addresses");

  // init slaves command
  auto init_slaves_command = MBusFrameFactory::create_nke_frame(MBusAddresses::NETWORK_LAYER);
  mbus->_protocol_handler->register_command(*init_slaves_command, scan_secondary_addresses_response_handler, /*step*/ 0,
                                            /*delay*/ 0, /*wait_for_response*/ false);
}

void MBus::scan_secondary_addresses_response_handler(MBusCommand *command, const MBusFrame &response) {
  ESP_LOGV(TAG, "scan_secondary_addresses_response_handler. step = %d", command->step);

  // init slaves command response
  if (command->step == 0) {
    ESP_LOGV(TAG, "send slave select command:");
    std::vector<uint8_t> mask{0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF};
    auto slave_select_command = MBusFrameFactory::create_slave_select(mask);
    command->mbus->_protocol_handler->register_command(*slave_select_command, scan_secondary_addresses_response_handler,
                                                       /*step*/ 1);
    return;
  }

  // init slaves OK -> send req_ud2 command
  if (command->step == 1 && response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_ACK) {
    ESP_LOGV(TAG, "send REQ_UD2 command:");
    auto req_ud2_command = MBusFrameFactory::create_req_ud2_frame();
    command->mbus->_protocol_handler->register_command(*req_ud2_command, scan_secondary_addresses_response_handler,
                                                       /*step*/ 2);
    return;
  }

  // receive req_ud2
  if (command->step == 2 && response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_LONG) {
    if (response.variable_data == nullptr) {
      return;
    }

    auto secondary_address = response.variable_data->header.get_secondary_address();
    ESP_LOGD(TAG, "Found a device on secondary address 0x%s.", secondary_address.c_str());
  }
}

}  // namespace mbus
}  // namespace esphome
