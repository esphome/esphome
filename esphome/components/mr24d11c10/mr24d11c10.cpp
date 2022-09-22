#include "mr24d11c10.h"
#include "esphome/core/log.h"
#include "crc.h"

namespace esphome {
namespace mr24d11c10 {

static const char *const TAG = "mr24d11c10";

static const int MESSAGE_HEAD = 0x55;

void MR24D11C10Sensor::setup() { ESP_LOGCONFIG(TAG, "Setting up MR24D11C10"); }

void MR24D11C10Sensor::dump_config() {
  LOG_BINARY_SENSOR("", "MR24D11C10", this);

  this->check_uart_settings(9600);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MR24D11C10 failed!");
  }
}

void MR24D11C10Sensor::set_req_key(bool x) { ESP_LOGD(TAG, "set_req_key"); }

void MR24D11C10Sensor::loop() {
  if (!this->available()) {
    return;
  }

  uint8_t byte = this->read();
  // ESP_LOGD(TAG, "Received byte: 0x%x", byte);
  // ESP_LOGD(TAG, "reading: %d", this->reading);
  // ESP_LOGD(TAG, "vector size: %d", this->packet.size());

  // detect malformed message start
  if (!this->reading && byte != MESSAGE_HEAD) {
    ESP_LOGW(TAG, "Malformed message, did not receive MESSAGE_HEAD, received: 0x%x", byte);
    return;
  }

  if (!this->reading) {
    // start new packet
    this->reading = true;
    this->expected_length = 0;
    this->packet.clear();
    ESP_LOGD(TAG, "Start of new packet detected");
  }

  // add received byte to packet
  this->packet.push_back(byte);

  uint16_t packet_size = this->packet.size();

  if (packet_size == 3) {
    // read expected packet length
    uint16_t length_low = this->packet.at(1);
    uint16_t length_high = this->packet.at(2);

    this->expected_length = (length_high << 8 | length_low) + 1;  // include MESSAGE_HEAD in length
    ESP_LOGD(TAG, "Expected packet length (including MSG_HEAD): %d", this->expected_length);
  }

  if (this->expected_length > 0 && packet_size >= this->expected_length) {
    // packet fully received
    // TODO: check packet CRC, decode packet, and more awesome stuff
    ESP_LOGD(TAG, "Packet fully received");

    this->reading = false;

    // check CRC checksum
    uint16_t crc16_high = this->packet.at(packet_size - 1);
    uint16_t crc16_low = this->packet.at(packet_size - 2);
    uint16_t packet_crc = crc16_low << 8 | crc16_high;  // NOTE: CRC calculation returns result as low-high

    uint8_t *data = this->packet.data();
    uint16_t calculated_crc = us_CalculateCrc16(data, packet_size - 2);

    ESP_LOGV(TAG, "Packet CRC: 0x%x, Calc. CRC: 0x%x", packet_crc, calculated_crc);

    if (calculated_crc != packet_crc) {
      ESP_LOGW(TAG, "CRC checksum failed, discarding received packet!");
      return;
    }
  }
}

}  // namespace mr24d11c10
}  // namespace esphome