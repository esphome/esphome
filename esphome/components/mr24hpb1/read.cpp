#include "mr24hpb1.h"
#include "constants.h"

namespace esphome {
namespace mr24hpb1 {
std::string MR24HPB1Component::read_device_id_() {
  // read Device ID
  ESP_LOGD(TAG, "Reading Device ID");
  this->write_packet_(READ_COMMAND, RC_MARKING_SEARCH, RC_MS_DEVICE_ID);

  std::vector<uint8_t> recv_packet;
  bool received = this->wait_for_packet_(recv_packet, PASSIVE_REPORTING, PR_REPORTING_MODULE_ID, PR_RMI_DEVICE_ID, 5);

  if (!received) {
    ESP_LOGE(TAG, "Device ID could not be read!");
    return "";
  }

  ESP_LOGD(TAG, "Received Device ID packet, length: %d", recv_packet.size());

  this->log_packet_(recv_packet);

  // read device id and convert to string
  std::string device_id = packet_data_to_string(recv_packet);

  ESP_LOGD(TAG, "Got Device ID=%s", device_id.c_str());

  return device_id;
}
}  // namespace mr24hpb1
}  // namespace esphome