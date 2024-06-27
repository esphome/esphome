#include "esphome/core/log.h"

#include "mbus.h"
#include "mbus_decoder.h"
#include "mbus_frame_factory.h"
#include "mbus_frame_meta.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus";

void MBus::dump_config() {
  ESP_LOGCONFIG(TAG, "MBus:");
  ESP_LOGCONFIG(TAG, "  Secondary Address: 0x%.016llX", this->secondary_address_);
  ESP_LOGCONFIG(TAG, "  delay: %d", this->interval_);
  ESP_LOGCONFIG(TAG, "  %d sensors configured", this->sensors_.size());
  for (auto index = 0; index < this->sensors_.size(); index++) {
    auto *sensor = this->sensors_.at(index);
    ESP_LOGCONFIG(TAG, "   %d. sensor: Data Index = %d, Factor = %f", index + 1, sensor->get_data_index(),
                  sensor->get_factor());
  }
}

float MBus::get_setup_priority() const {
  // After UART bus
  return setup_priority::LATE;
}

void MBus::setup() {
  if (this->secondary_address_ == 0) {
    start_scan_primary_addresses(*this);
  } else {
    start_reading_data(*this);
  }
}

void MBus::loop() { this->protocol_handler_.loop(); }

void MBus::start_scan_primary_addresses(MBus &mbus) {
  ESP_LOGV(TAG, "start_scan_primary_addresses: %.2X", mbus.primary_address_);

  // MBus spec states to send REQ_UD2 but sending init-frame is much faster
  // see Chapter 7.3 Primary Addresses
  auto scan_primary_command = MBusFrameFactory::create_nke_frame(mbus.primary_address_);
  mbus.protocol_handler_.register_command(*scan_primary_command, scan_primary_addresses_response_handler,
                                          /*step*/ 0, /*delay*/ 1000);
}

void MBus::scan_primary_addresses_response_handler(const MBusCommand &command, const MBusFrame &response) {
  auto *mbus = command.mbus;

  if (command.step == 0 && response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_ACK) {
    ESP_LOGD(TAG, "Found mbus device with primary address = %.2X.", mbus->primary_address_);
    start_scan_secondary_addresses(*mbus);
    return;
  }

  mbus->primary_address_++;
  if (mbus->primary_address_ > PRIMARY_ADDRESS_MAX) {
    ESP_LOGE(TAG, "No Device with primary address found.");
    return;
  }

  ESP_LOGV(TAG, "scan_primary_addresses: %.2X", mbus->primary_address_);
  auto scan_primary_command = MBusFrameFactory::create_nke_frame(mbus->primary_address_);
  mbus->protocol_handler_.register_command(*scan_primary_command, scan_primary_addresses_response_handler,
                                           /*step*/ 0, /*delay*/ 1000);
}

void MBus::start_scan_secondary_addresses(MBus &mbus) {
  ESP_LOGV(TAG, "start_scan_secondary_addresses");

  // init command
  auto init_command = MBusFrameFactory::create_nke_frame(MBusAddresses::NETWORK_LAYER);
  mbus.protocol_handler_.register_command(*init_command, scan_secondary_addresses_response_handler, /*step*/ 0,
                                          /*delay*/ 0, /*wait_for_response*/ false);
}

void MBus::scan_secondary_addresses_response_handler(const MBusCommand &command, const MBusFrame &response) {
  ESP_LOGV(TAG, "scan_secondary_addresses_response_handler. step = %d, frame_type = %d", command.step,
           response.frame_type);

  auto *mbus = command.mbus;

  // init command response
  if (command.step == 0) {
    ESP_LOGV(TAG, "send device select command:");
    uint64_t mask = 0x0FFFFFFFFFFFFFFF;
    auto device_select_command = MBusFrameFactory::create_select_frame(mask);
    mbus->protocol_handler_.register_command(*device_select_command, scan_secondary_addresses_response_handler,
                                             /*step*/ 1);
    return;
  }

  // init device OK -> send req_ud2 command
  if (command.step == 1 && response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_ACK) {
    ESP_LOGV(TAG, "send REQ_UD2 command:");
    auto req_ud2_command = MBusFrameFactory::create_req_ud2_frame();
    mbus->protocol_handler_.register_command(*req_ud2_command, scan_secondary_addresses_response_handler,
                                             /*step*/ 2);
    return;
  }

  // receive req_ud2
  if (command.step == 2 && response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_LONG) {
    if (response.variable_data == nullptr) {
      return;
    }

    auto header = response.variable_data->header;
    auto secondary_address =
        MBusDecoder::decode_secondary_address(header.id, header.manufacturer, header.version, header.medium);
    // auto secondary_address = response.variable_data->header.get_secondary_address();
    ESP_LOGD(TAG, "Found a device on secondary address 0x%.016llX.", secondary_address);
    response.dump();
  }
}

void MBus::start_reading_data(MBus &mbus) {
  ESP_LOGV(TAG, "start_reading_data from secondary_address: 0x%.016llX", mbus.secondary_address_);

  // init device command
  auto init_device_command = MBusFrameFactory::create_nke_frame(MBusAddresses::NETWORK_LAYER);
  mbus.protocol_handler_.register_command(*init_device_command, reading_data_response_handler, /*step*/ 0,
                                          /*delay*/ 1000, /*wait_for_response*/ false);
}

void MBus::reading_data_response_handler(const MBusCommand &command, const MBusFrame &response) {
  ESP_LOGV(TAG, "reading_data_response_handler. step = %d, frame_type = %d", command.step, response.frame_type);

  auto *mbus = command.mbus;
  // select device by secondary address
  if (command.step == 0) {
    ESP_LOGV(TAG, "send device select command:");
    auto device_select_command = MBusFrameFactory::create_select_frame(mbus->secondary_address_);
    mbus->protocol_handler_.register_command(*device_select_command, reading_data_response_handler,
                                             /*step*/ 1);
    return;
  }

  // create read data request #1
  if (command.step == 1 && (response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_ACK)) {
    ESP_LOGV(TAG, "send REQ_UD2 command #1:");
    auto req_ud2_command = MBusFrameFactory::create_req_ud2_frame();
    mbus->protocol_handler_.register_command(*req_ud2_command, reading_data_response_handler,
                                             /*step*/ 2);
    return;
  }

  // create read data request #2
  if (command.step == 2 && (response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_ACK ||
                            response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_LONG)) {
    ESP_LOGV(TAG, "send REQ_UD2 command #2:");
    auto req_ud2_command = MBusFrameFactory::create_req_ud2_frame();
    mbus->protocol_handler_.register_command(*req_ud2_command, reading_data_response_handler,
                                             /*step*/ 3);
    return;
  }

  // parse response and send read data request (loop)
  if (command.step == 3) {
    if (response.frame_type == MBusFrameType::MBUS_FRAME_TYPE_LONG && !mbus->sensors_.empty() &&
        response.variable_data && !response.variable_data->records.empty()) {
      auto data_size = response.variable_data->records.size();
      for (auto *sensor : mbus->sensors_) {
        auto data_index = sensor->get_data_index();
        if (data_index >= data_size) {
          continue;
        }

        auto record = response.variable_data->records.at(data_index);
        auto value = record.parse(data_index);
        sensor->publish(value);
      }
    }
    auto req_ud2_command = MBusFrameFactory::create_req_ud2_frame();
    mbus->protocol_handler_.register_command(*req_ud2_command, reading_data_response_handler,
                                             /*step*/ 3, /*delay*/ mbus->interval_ * 1000);
    return;
  }

  ESP_LOGE(TAG, "Error in reading data loop. Unexpected frame:");
  // response.dump();
}

}  // namespace mbus
}  // namespace esphome
