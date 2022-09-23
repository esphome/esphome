#include "mr24hpb1.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "crc.h"
#include "constants.h"
#include <iomanip>
namespace esphome {
namespace mr24hpb1 {
void MR24HPB1Component::setup() {
  // creating a list of all system information that needs to be fetched only once after startup
  this->system_information_sensors.push_back(std::make_tuple(this->device_id_sensor_, RC_MARKING_SEARCH, RC_MS_DEVICE_ID));
  this->system_information_sensors.push_back(std::make_tuple(this->software_version_sensor_, RC_MARKING_SEARCH, RC_MS_SOFTWARE_VERSION));
  this->system_information_sensors.push_back(std::make_tuple(this->hardware_version_sensor_, RC_MARKING_SEARCH, RC_MS_HARDWARE_VERSION));
  this->system_information_sensors.push_back(std::make_tuple(this->protocol_version_sensor_, RC_MARKING_SEARCH, RC_MS_PROTOCOL_VERSION));

  ESP_LOGCONFIG(TAG, "Setting up MR24HPB1");

  this->check_uart_settings(9600);

  // read device ID
  uint8_t num_tries = 0;
  while (num_tries < 5) {
    std::string device_id = this->read_device_id();
    if (device_id != "") {
      this->device_id_sensor_->publish_state(device_id);
      break;
    }
    num_tries++;
  }
  if (num_tries >= 5) {
    this->mark_failed();
  }

  // set threshold gear
  this->write_threshold_gear(this->threshold_gear_);
  std::vector<uint8_t> recv_packet;
  bool received =
      this->wait_for_packet(recv_packet, PASSIVE_REPORTING, PR_REPORTING_SYSTEM_INFO, PR_RSI_THRESHOLD_GEAR, 10);
  if (!received) {
    ESP_LOGE(TAG, "Threshold gear write not acknowledged!");
    this->mark_failed();
  } else {
    uint8_t threshold_gear = packet_data_to_int(recv_packet);
    if (this->threshold_gear_ == threshold_gear) {
      ESP_LOGD(TAG, "Threshold gear set acknowledged");
    } else {
      ESP_LOGE(TAG, "Reported threshold gear not equal to written value, got: %d!", threshold_gear);
      this->mark_failed();
    }
  }

  // set scene setting
  this->write_scene_setting(this->scene_setting_);
  recv_packet.clear();
  received = this->wait_for_packet(recv_packet, PASSIVE_REPORTING, PR_REPORTING_SYSTEM_INFO, PR_RSI_SCENE_SETTINGS, 10);
  if (!received) {
    ESP_LOGE(TAG, "Scene setting write not acknowledged!");
    this->mark_failed();
  } else {
    uint8_t scene_setting = packet_data_to_int(recv_packet);
    if (this->scene_setting_ == scene_setting) {
      ESP_LOGD(TAG, "Scene setting set acknowledged");
    } else {
      ESP_LOGE(TAG, "Reported scene setting not equal to written value, got: %d!", scene_setting);
      this->mark_failed();
    }
  }

  // set force unoccupied time
  this->write_force_unoccupied_setting(this->forced_unoccupied_);
  recv_packet.clear();
  received = this->wait_for_packet(recv_packet, PASSIVE_REPORTING, PR_REPORTING_SYSTEM_INFO, PR_RSI_FORCED_UNOCCUPIED_SETTINGS, 10);
  if (!received) {
    ESP_LOGE(TAG, "Forced unoccupied write not acknowledged!");
    this->mark_failed();
  } else {
    ForcedUnoccupied forced_unoccupied_setting = static_cast<ForcedUnoccupied>(packet_data_to_int(recv_packet));
    if (this->forced_unoccupied_ == forced_unoccupied_setting) {
      ESP_LOGD(TAG, "Forced unoccupied set acknowledged");
    } else {
      ESP_LOGE(TAG, "Reported forced unoccupied not equal to written value, got: %d!", static_cast<uint8_t>(forced_unoccupied_setting));
      this->mark_failed();
    }
  }
}

float MR24HPB1Component::get_setup_priority() const { return setup_priority::DATA; }

void MR24HPB1Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MR24HPB1:");

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MR24HPB1 failed!");
  }

  ESP_LOGCONFIG(TAG, "Scene Setting: %s", SceneSetting_to_string(this->scene_setting_));
  ESP_LOGCONFIG(TAG, "Threshold Gear: %d", this->threshold_gear_);
  LOG_TEXT_SENSOR("  ", "Device ID:", this->device_id_sensor_);
  LOG_TEXT_SENSOR("  ", "Software Version:", this->software_version_sensor_);
  LOG_TEXT_SENSOR("  ", "Hardware Version:", this->hardware_version_sensor_);
  LOG_TEXT_SENSOR("  ", "Protocol Version:", this->protocol_version_sensor_);
  LOG_TEXT_SENSOR("  ", "Environment Status:", this->environment_status_sensor_);
  LOG_BINARY_SENSOR("  ", "Occupancy:", this->occupancy_sensor_);
  LOG_BINARY_SENSOR("  ", "Movement:", this->movement_sensor_);
  LOG_SENSOR("  ", "Movement Rate:", this->movement_rate_sensor_);
}

void MR24HPB1Component::loop() {
  if (!info_fully_populated) {
    this->get_general_infos();
  }
  reception_status stat = this->receive_packet(this->current_packet);
  if (stat == MR24HPB1Component::COMPLETE) {
    FunctionCode current_func_code = get_packet_function_code(this->current_packet);
    switch (current_func_code) {
      case PASSIVE_REPORTING:
        this->handle_passive_reporting(this->current_packet);
        break;
      case PROACTIVE_REPORTING:
        this->handle_active_reporting(this->current_packet);
        break;
      case SLEEPING_DATA_REPORTING:
        this->handle_sleep_data_report(this->current_packet);
        break;
      case FALL_DETECTION_DATA_REPORTING:
        this->handle_fall_data_report(this->current_packet);
        break;
      default:
        ESP_LOGW(TAG, "Packet had unkown function code: 0x%x", current_func_code);
        this->log_packet(this->current_packet);
        break;
    }
    // ESP_LOGD(TAG, "Packet fully received");
    this->current_packet.clear();
  }
}

void MR24HPB1Component::get_general_infos() {
  info_fully_populated = true;
  for(auto item : this->system_information_sensors) {
    text_sensor::TextSensor* current_sesnor = std::get<0>(item);
    if(current_sesnor != nullptr && current_sesnor->get_state() == "" && this->respone_requested == 0) {
      this->respone_requested = millis();
      this->write_packet(READ_COMMAND, std::get<1>(item), std::get<2>(item));
    }
    info_fully_populated = false;
  }
  if (millis() > this->respone_requested + PACKET_WAIT_TIMEOUT_MS) {
    this->respone_requested = 0;
  }
}

void MR24HPB1Component::write_packet(FunctionCode function_code, AddressCode1 address_code_1,
                                     AddressCode2 address_code_2, std::vector<uint8_t> &data) {
  uint16_t packet_size = data.size() + 8;
  std::vector<uint8_t> packet;
  packet.reserve(packet_size);

  packet.push_back(PACKET_START);
  packet.push_back(packet_size & 0x00FF);
  packet.push_back(packet_size >> 8);
  packet.push_back(function_code);
  packet.push_back(address_code_1);
  packet.push_back(address_code_2);
  packet.insert(std::end(packet), std::begin(data), std::end(data));

  uint16_t crc = us_CalculateCrc16(packet.data(), packet_size - 2);

  packet.push_back(crc >> 8);
  packet.push_back(crc & 0x00FF);

  this->write_array(packet);

  this->log_packet(packet);
}

void MR24HPB1Component::write_packet(FunctionCode function_code, AddressCode1 address_code_1,
                                     AddressCode2 address_code_2) {
  std::vector<uint8_t> empty_data;
  MR24HPB1Component::write_packet(function_code, address_code_1, address_code_2, empty_data);
}

void MR24HPB1Component::log_packet(std::vector<uint8_t> &packet) {
  char* buf = new char[packet.size() * 5 + 8];
  uint16_t counter = 7;

  sprintf(buf, "Packet:");
  for (uint8_t &byte : packet) {
    sprintf(&(buf[counter]), " 0x%02x", byte);
    counter += 5;
  }

  ESP_LOGD(TAG, buf);
  delete[] buf;
}

bool MR24HPB1Component::wait_for_packet(std::vector<uint8_t> &packet, FunctionCode function_code,
                                        AddressCode1 address_code_1, AddressCode2 address_code_2, uint8_t timeout_s) {
  uint32_t starting_timestamp = millis();
  reception_status stat;
  while (millis() < starting_timestamp + timeout_s * 1000) {
    stat = this->receive_packet(packet);
    if (stat == MR24HPB1Component::COMPLETE) {
      if (get_packet_function_code(packet) == function_code && get_packet_address_code_1(packet) == address_code_1 &&
          get_packet_address_code_2(packet) == address_code_2) {
        ESP_LOGD(TAG, "Found expected packet");

        return true;
      } else {
        // ESP_LOGD(TAG, "Other packet received");
        // this->log_packet(packet);
        packet.clear();
      }
    }
    App.feed_wdt();
  }

  return false;
}

MR24HPB1Component::reception_status MR24HPB1Component::receive_packet(std::vector<uint8_t> &packet) {
  // Frame strucutre:
  // +----------+---------------------+----------------+----------------+----------------+---------+-------------------+
  // | Starting | Length of data      | Function codes | Address code 1 | Address code 2 | Data    | Check code        |
  // | Code     |                     |                |                |                |         |                   |
  // +----------+----------+----------+----------------+----------------+----------------+---------+---------+---------+
  // | 0x55     | Length_L | Length_H | Command        | Address_1      | Address_2      | Data    | Crc16_L | Crc16_H |
  // +----------+----------+----------+----------------+----------------+----------------+---------+---------+---------+
  // | 1 Byte   | 1 Byte   | 1 Byte   | 1 Byte         | 1 Byte         | 1 Byte         | n Bytes | 1 Byte  | 1 Byte  |
  // +----------+----------+----------+----------------+----------------+----------------+---------+---------+---------+
  if (!this->available()) {
    return this->current_receive_status;
  }

  uint8_t byte = this->read();
  // ESP_LOGD(TAG, "Received byte: 0x%x", byte);
  // ESP_LOGD(TAG, "vector size: %d", packet.size());

  // detect malformed message start
  if (this->current_receive_status == MR24HPB1Component::WAITING && byte != PACKET_START) {
    ESP_LOGW(TAG, "Malformed message, did not receive PACKET_START, received: 0x%x", byte);
    return MR24HPB1Component::MALFORMED_PACKET;
  }

  if (this->current_receive_status == MR24HPB1Component::WAITING) {
    // start new packet
    this->current_receive_status = MR24HPB1Component::RECEIVING;
    this->expected_length = 0;
    // ESP_LOGD(TAG, "Start of new packet detected");
  }

  // add received byte to packet
  packet.push_back(byte);

  uint16_t packet_size = packet.size();

  // ESP_LOGD(TAG, "packet_size=%d, expected_size=%d", packet_size, this->expected_length);

  if (packet_size == 3) {
    // read expected packet length
    this->expected_length = get_packet_length(packet);
    // ESP_LOGD(TAG, "Expected packet length (including MSG_HEAD): %d", this->expected_length);
  }

  if (this->expected_length > 0 && packet_size >= this->expected_length) {
    // packet fully received

    this->current_receive_status = MR24HPB1Component::WAITING;

    // check CRC checksum
    uint16_t packet_crc = get_packet_crc(packet);

    uint8_t *data = packet.data();
    uint16_t calculated_crc = us_CalculateCrc16(data, packet_size - 2);

    ESP_LOGV(TAG, "Packet CRC: 0x%x, Calc. CRC: 0x%x", packet_crc, calculated_crc);

    if (calculated_crc != packet_crc) {
      ESP_LOGW(TAG, "CRC checksum failed, discarding received packet!");
      return MR24HPB1Component::CRC_ERROR;
    } else {
      this->current_receive_status = MR24HPB1Component::WAITING;
      return MR24HPB1Component::COMPLETE;
    }
  }
  return this->current_receive_status;
}

}  // namespace mr24hpb1
}  // namespace esphome