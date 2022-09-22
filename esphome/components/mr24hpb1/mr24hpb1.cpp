#include "mr24hpb1.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "crc.h"
#include "constants.h"
#include <sstream>
#include <iomanip>
namespace esphome {
namespace mr24hpb1 {

static const char *const TAG = "mr24hpb1";

static FunctionCode get_packet_function_code(std::vector<uint8_t> &packet) { return packet.at(3); }

static AddressCode1 get_packet_address_code_1(std::vector<uint8_t> &packet) { return packet.at(4); }

static AddressCode2 get_packet_address_code_2(std::vector<uint8_t> &packet) { return packet.at(5); }

static uint16_t get_packet_length(std::vector<uint8_t> &packet) {
  uint16_t length_low = packet.at(1);
  uint16_t length_high = packet.at(2);

  return (length_high << 8 | length_low) + 1;  // include PACKET_START in length
}

static uint16_t get_packet_crc(std::vector<uint8_t> &packet) {
  uint16_t packet_size = packet.size();
  uint16_t crc16_high = packet.at(packet_size - 1);
  uint16_t crc16_low = packet.at(packet_size - 2);
  return crc16_low << 8 | crc16_high;
}

static std::vector<uint8_t> get_packet_data(std::vector<uint8_t> &packet) {
  return std::vector<uint8_t>(packet.begin() + 6, packet.end() - 2);
}

void MR24HPB1Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MR24HPB1");

  this->check_uart_settings(9600);

  // read device ID
  std::string device_id = this->read_device_id();
  if (device_id != "") {
    if (this->device_id_sensor_ != nullptr) {
      this->device_id_sensor_->publish_state(device_id);
    }
  } else {
    this->mark_failed();
  }

  // read Software Version
  std::string software_version = this->read_software_version();
  if (software_version != "") {
    if (this->software_version_sensor_ != nullptr) {
      this->software_version_sensor_->publish_state(software_version);
    }
  } else {
    this->mark_failed();
  }
}

float MR24HPB1Component::get_setup_priority() const { return setup_priority::DATA; }

std::string MR24HPB1Component::read_software_version() {
  ESP_LOGD(TAG, "Reading software version");
  this->write_packet(READ_COMMAND, RC_MARKING_SEARCH, RC_MS_SOFTWARE_VERSION);

  std::vector<uint8_t> recv_packet;
  bool received =
      this->wait_for_packet(recv_packet, PASSIVE_REPORTING, PR_REPORTING_MODULE_ID, PR_RMI_SOFTWARE_VERSION, 10);

  if (!received) {
    ESP_LOGE(TAG, "Software version could not be read!");
    return "";
  }

  ESP_LOGD(TAG, "Received software version packet, length: %d", recv_packet.size());

  this->log_packet(recv_packet);

  // read software version and convert to string
  std::stringstream ss;
  std::vector<uint8_t> data = get_packet_data(recv_packet);
  for (uint8_t &byte : data) {
    ss << byte;
  }
  std::string software_version = ss.str();

  ESP_LOGD(TAG, "Got software version=%s", software_version.c_str());
  return software_version;
}

std::string MR24HPB1Component::read_device_id() {
  // read Device ID
  ESP_LOGD(TAG, "Reading Device ID");
  this->write_packet(READ_COMMAND, RC_MARKING_SEARCH, RC_MS_DEVICE_ID);

  std::vector<uint8_t> recv_packet;
  bool received = this->wait_for_packet(recv_packet, PASSIVE_REPORTING, PR_REPORTING_MODULE_ID, PR_RMI_DEVICE_ID, 5);

  if (!received) {
    ESP_LOGE(TAG, "Device ID could not be read!");
    return "";
  }

  ESP_LOGD(TAG, "Received Device ID packet, length: %d", recv_packet.size());

  this->log_packet(recv_packet);

  // read device id and convert to string
  std::stringstream ss;
  std::vector<uint8_t> data = get_packet_data(recv_packet);
  for (uint8_t &byte : data) {
    ss << byte;
  }
  std::string device_id = ss.str();

  ESP_LOGD(TAG, "Got Device ID=%s", device_id.c_str());

  return device_id;
}

void MR24HPB1Component::dump_config() {
  ESP_LOGD(TAG, "MR24HPB1:");

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MR24HPB1 failed!");
  }

  LOG_TEXT_SENSOR("  ", "Device ID", this->device_id_sensor_);
  LOG_TEXT_SENSOR("  ", "Software Version", this->software_version_sensor_);
}

void MR24HPB1Component::loop() {
  // reception_status stat = this->receive_packet(this->packet);
  // if (stat == MR24HPB1Component::COMPLETE) {
  //   ESP_LOGD(TAG, "Packet fully received");
  //   // TODO: decode packet, and more awesome stuff
  //   this->packet.clear();
  // }
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
  std::stringstream ss;

  ss << "Packet:";
  for (uint8_t &byte : packet) {
    ss << " " << std::hex << static_cast<int>(byte);
  }

  ss.flush();
  ESP_LOGD(TAG, ss.str().c_str());
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
        packet.clear();
        this->log_packet(packet);
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
  // ESP_LOGD(TAG, "reading: %d", this->reading);
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
    ESP_LOGD(TAG, "Start of new packet detected");
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