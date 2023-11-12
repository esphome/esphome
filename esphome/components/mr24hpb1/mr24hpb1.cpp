#include "mr24hpb1.h"
#include "constants.h"
#include "crc.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mr24hpb1 {
void MR24HPB1Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MR24HPB1");

#ifdef USE_TEXT_SENSOR
  // creating a list of all system information that needs to be fetched only once after startup
  if (this->device_id_sensor_ != nullptr)

    this->system_information_sensors_.emplace_back(this->device_id_sensor_, RC_MARKING_SEARCH, RC_MS_DEVICE_ID);
  if (this->software_version_sensor_ != nullptr)
    this->system_information_sensors_.emplace_back(this->software_version_sensor_, RC_MARKING_SEARCH,
                                                   RC_MS_SOFTWARE_VERSION);
  if (this->hardware_version_sensor_ != nullptr)
    this->system_information_sensors_.emplace_back(this->hardware_version_sensor_, RC_MARKING_SEARCH,
                                                   RC_MS_HARDWARE_VERSION);
  if (this->protocol_version_sensor_ != nullptr)
    this->system_information_sensors_.emplace_back(this->protocol_version_sensor_, RC_MARKING_SEARCH,
                                                   RC_MS_PROTOCOL_VERSION);
#endif

  this->check_uart_settings(9600);

  // read device ID
  uint8_t num_tries = 0;
  while (num_tries < 5) {
    std::string device_id = this->read_device_id_();
    if (!device_id.empty()) {
#ifdef USE_TEXT_SENSOR
      if (this->device_id_sensor_ != nullptr) {
        this->device_id_sensor_->publish_state(device_id);
      }
#endif
      break;
    }
    num_tries++;
  }
  if (num_tries >= 5) {
    this->mark_failed();
    return;
  }

  // set threshold gear
  this->write_threshold_gear_(this->threshold_gear_);
  Packet recv_packet;
  bool received =
      this->wait_for_packet_(recv_packet, PASSIVE_REPORTING, PR_REPORTING_SYSTEM_INFO, PR_RSI_THRESHOLD_GEAR, 10);
  if (!received) {
    ESP_LOGE(TAG, "Threshold gear write not acknowledged!");
    this->mark_failed();
    return;
  } else {
    uint8_t threshold_gear = recv_packet.data_as_int();
    if (this->threshold_gear_ == threshold_gear) {
      ESP_LOGD(TAG, "Threshold gear set acknowledged");
    } else {
      ESP_LOGE(TAG, "Reported threshold gear not equal to written value, got: %d!", threshold_gear);
      this->mark_failed();
      return;
    }
  }

  // set scene setting
  this->write_scene_setting_(this->scene_setting_);
  recv_packet.clear();
  received =
      this->wait_for_packet_(recv_packet, PASSIVE_REPORTING, PR_REPORTING_SYSTEM_INFO, PR_RSI_SCENE_SETTINGS, 10);
  if (!received) {
    ESP_LOGE(TAG, "Scene setting write not acknowledged!");
    this->mark_failed();
    return;
  } else {
    uint8_t scene_setting = recv_packet.data_as_int();
    if (this->scene_setting_ == scene_setting) {
      ESP_LOGD(TAG, "Scene setting set acknowledged");
    } else {
      ESP_LOGE(TAG, "Reported scene setting not equal to written value, got: %d!", scene_setting);
      this->mark_failed();
      return;
    }
  }

  // set force unoccupied time
  this->write_force_unoccupied_setting_(this->forced_unoccupied_);
  recv_packet.clear();
  received = this->wait_for_packet_(recv_packet, PASSIVE_REPORTING, PR_REPORTING_SYSTEM_INFO,
                                    PR_RSI_FORCED_UNOCCUPIED_SETTINGS, 10);
  if (!received) {
    ESP_LOGE(TAG, "Forced unoccupied write not acknowledged!");
    this->mark_failed();
    return;
  } else {
    ForcedUnoccupied forced_unoccupied_setting = static_cast<ForcedUnoccupied>(recv_packet.data_as_int());
    if (this->forced_unoccupied_ == forced_unoccupied_setting) {
      ESP_LOGD(TAG, "Forced unoccupied set acknowledged");
    } else {
      ESP_LOGE(TAG, "Reported forced unoccupied not equal to written value, got: %d!",
               static_cast<uint8_t>(forced_unoccupied_setting));
      this->mark_failed();
      return;
    }
  }
}

float MR24HPB1Component::get_setup_priority() const { return setup_priority::DATA; }

void MR24HPB1Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MR24HPB1:");

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MR24HPB1 failed!");
  }

  ESP_LOGCONFIG(TAG, "Scene Setting: %s", scene_setting_to_string(this->scene_setting_));
  ESP_LOGCONFIG(TAG, "Threshold Gear: %d", this->threshold_gear_);
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Device ID:", this->device_id_sensor_);
  LOG_TEXT_SENSOR("  ", "Software Version:", this->software_version_sensor_);
  LOG_TEXT_SENSOR("  ", "Hardware Version:", this->hardware_version_sensor_);
  LOG_TEXT_SENSOR("  ", "Protocol Version:", this->protocol_version_sensor_);
  LOG_TEXT_SENSOR("  ", "Environment Status:", this->environment_status_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Occupancy:", this->occupancy_sensor_);
  LOG_BINARY_SENSOR("  ", "Movement:", this->movement_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Movement Rate:", this->movement_rate_sensor_);
#endif
}

void MR24HPB1Component::loop() {
#ifdef USE_TEXT_SENSOR
  if (!info_fully_populated_) {
    this->get_general_infos_();
  }
#endif
  ReceptionStatus stat = this->receive_packet_(this->current_packet_);
  if (stat == MR24HPB1Component::COMPLETE) {
    FunctionCode current_func_code = this->current_packet_.function_code();
    switch (current_func_code) {
      case PASSIVE_REPORTING:
        this->handle_passive_reporting_(this->current_packet_);
        break;
      case PROACTIVE_REPORTING:
        this->handle_active_reporting_(this->current_packet_);
        break;
      case SLEEPING_DATA_REPORTING:
        this->handle_sleep_data_report_(this->current_packet_);
        break;
      case FALL_DETECTION_DATA_REPORTING:
        this->handle_fall_data_report_(this->current_packet_);
        break;
      default:
        ESP_LOGW(TAG, "Packet had unknown function code: 0x%x", current_func_code);
        this->current_packet_.log();
        break;
    }
    // ESP_LOGD(TAG, "Packet fully received");
    this->current_packet_.clear();
  }
}

#ifdef USE_TEXT_SENSOR
void MR24HPB1Component::get_general_infos_() {
  info_fully_populated_ = true;
  for (auto item : this->system_information_sensors_) {
    text_sensor::TextSensor *current_sensor = std::get<0>(item);
    if (current_sensor->get_state().empty() && this->response_requested_ == 0) {
      this->response_requested_ = millis();
      this->write_packet_(READ_COMMAND, std::get<1>(item), std::get<2>(item));
      info_fully_populated_ = false;
    }
  }
  if (millis() > this->response_requested_ + PACKET_WAIT_TIMEOUT_MS) {
    this->response_requested_ = 0;
  }
}
#endif

void MR24HPB1Component::write_packet_(FunctionCode function_code, AddressCode1 address_code_1,
                                      AddressCode2 address_code_2, std::vector<uint8_t> &data) {
  uint16_t packet_size = data.size() + 8;
  Packet packet(packet_size);

  packet.push_data(PACKET_START);
  packet.push_data(packet_size & 0x00FF);
  packet.push_data(packet_size >> 8);
  packet.push_data(function_code);
  packet.push_data(address_code_1);
  packet.push_data(address_code_2);
  packet.append_data(data);

  uint16_t crc = us_calculate_crc16(packet.raw_data(), packet_size - 2);

  packet.push_data(crc >> 8);
  packet.push_data(crc & 0x00FF);

  this->write_array(packet.raw_vect());

  packet.log();
}

void MR24HPB1Component::write_packet_(FunctionCode function_code, AddressCode1 address_code_1,
                                      AddressCode2 address_code_2) {
  std::vector<uint8_t> empty_data;
  MR24HPB1Component::write_packet_(function_code, address_code_1, address_code_2, empty_data);
}

bool MR24HPB1Component::wait_for_packet_(Packet &packet, FunctionCode function_code, AddressCode1 address_code_1,
                                         AddressCode2 address_code_2, uint8_t timeout_s) {
  uint32_t starting_timestamp = millis();
  ReceptionStatus stat;
  while (millis() < starting_timestamp + timeout_s * 1000) {
    stat = this->receive_packet_(packet);
    if (stat == MR24HPB1Component::COMPLETE) {
      if (packet.function_code() == function_code && packet.address_code_1() == address_code_1 &&
          packet.address_code_2() == address_code_2) {
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

MR24HPB1Component::ReceptionStatus MR24HPB1Component::receive_packet_(Packet &packet) {
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
    return this->current_receive_status_;
  }

  uint8_t byte = this->read();
  // ESP_LOGD(TAG, "Received byte: 0x%x", byte);
  // ESP_LOGD(TAG, "vector size: %d", packet.size());

  // detect malformed message start
  if (this->current_receive_status_ == MR24HPB1Component::WAITING && byte != PACKET_START) {
    ESP_LOGW(TAG, "Malformed message, did not receive PACKET_START, received: 0x%x", byte);
    return MR24HPB1Component::MALFORMED_PACKET;
  }

  if (this->current_receive_status_ == MR24HPB1Component::WAITING) {
    // start new packet
    this->current_receive_status_ = MR24HPB1Component::RECEIVING;
    this->expected_length_ = 0;
    // ESP_LOGD(TAG, "Start of new packet detected");
  }

  // add received byte to packet
  packet.push_data(byte);

  uint16_t packet_size = packet.raw_size();

  // ESP_LOGD(TAG, "packet_size=%d, expected_size=%d", packet_size, this->expected_length);

  if (packet_size == 3) {
    // read expected packet length
    this->expected_length_ = packet.length();
    // ESP_LOGD(TAG, "Expected packet length (including MSG_HEAD): %d", this->expected_length);
  }

  if (this->expected_length_ > 0 && packet_size >= this->expected_length_) {
    // packet fully received

    this->current_receive_status_ = MR24HPB1Component::WAITING;

    // check CRC checksum
    uint16_t packet_crc = packet.crc();

    uint8_t *data = packet.raw_data();
    uint16_t calculated_crc = us_calculate_crc16(data, packet_size - 2);

    ESP_LOGV(TAG, "Packet CRC: 0x%x, Calc. CRC: 0x%x", packet_crc, calculated_crc);

    if (calculated_crc != packet_crc) {
      ESP_LOGW(TAG, "CRC checksum failed, discarding received packet!");
      return MR24HPB1Component::CRC_ERROR;
    } else {
      this->current_receive_status_ = MR24HPB1Component::WAITING;
      return MR24HPB1Component::COMPLETE;
    }
  }
  return this->current_receive_status_;
}

}  // namespace mr24hpb1
}  // namespace esphome
