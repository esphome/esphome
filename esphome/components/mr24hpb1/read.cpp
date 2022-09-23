#include "mr24hpb1.h"
#include "constants.h"
#include <sstream>

namespace esphome {
namespace mr24hpb1 {
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
  std::string device_id = packet_data_to_string(recv_packet);

  ESP_LOGD(TAG, "Got Device ID=%s", device_id.c_str());

  return device_id;
}

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
  std::string software_version = packet_data_to_string(recv_packet);

  ESP_LOGD(TAG, "Got software version=%s", software_version.c_str());
  return software_version;
}

std::string MR24HPB1Component::read_hardware_version() {
  ESP_LOGD(TAG, "Reading hardware version");
  this->write_packet(READ_COMMAND, RC_MARKING_SEARCH, RC_MS_HARDWARE_VERSION);

  std::vector<uint8_t> recv_packet;
  bool received =
      this->wait_for_packet(recv_packet, PASSIVE_REPORTING, PR_REPORTING_MODULE_ID, PR_RMI_HARDWARE_VERSION, 10);

  if (!received) {
    ESP_LOGE(TAG, "Hardware version could not be read!");
    return "";
  }

  ESP_LOGD(TAG, "Received hardware version packet, length: %d", recv_packet.size());

  this->log_packet(recv_packet);

  // read hardware version and convert to string
  std::string hardware_version = packet_data_to_string(recv_packet);

  ESP_LOGD(TAG, "Got hardware version=%s", hardware_version.c_str());
  return hardware_version;
}

std::string MR24HPB1Component::read_protocol_version() {
  ESP_LOGD(TAG, "Reading protocol version");
  this->write_packet(READ_COMMAND, RC_MARKING_SEARCH, RC_MS_PROTOCOL_VERSION);

  std::vector<uint8_t> recv_packet;
  bool received =
      this->wait_for_packet(recv_packet, PASSIVE_REPORTING, PR_REPORTING_MODULE_ID, PR_RMI_PROTOCOL_VERSION, 10);

  if (!received) {
    ESP_LOGE(TAG, "Protocol version could not be read!");
    return "";
  }

  ESP_LOGD(TAG, "Received protocol version packet, length: %d", recv_packet.size());

  this->log_packet(recv_packet);

  // read protocol version and convert to string
  uint32_t protocol_version = packet_data_to_int(recv_packet);

  ESP_LOGD(TAG, "Got protocol version=%d", protocol_version);
  return esphome::to_string(protocol_version);
}
}  // namespace mr24hpb1
}  // namespace esphome