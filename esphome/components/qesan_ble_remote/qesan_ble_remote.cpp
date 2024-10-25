#include "qesan_ble_remote.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <esp_bt_defs.h>

namespace esphome {
namespace qesan_ble_remote {

static const char *const TAG = "qesan_ble_remote";

#ifdef USE_BINARY_SENSOR
bool QesanBinarySensor::on_update_received(uint8_t button_code, bool pressed) {
  bool match = (button_code == this->button_code_);
  if (match && !pressed) {
    // Make sure the binary sensor is turned on before turning it off
    this->publish_state(true);
  }
  this->publish_state(match && pressed);
  return match;
}
#endif  // USE_BINARY_SENSOR

void QesanListener::update() {
#ifdef USE_BINARY_SENSOR
  if ((this->last_message_received_ != 0) && (millis() - this->last_message_received_ > 1000)) {
    // Turn off all binary sensors when no messages were received for a while
    for (auto *binary_sensor : this->binary_sensors_) {
      binary_sensor->publish_state(false);
    }
    this->last_message_received_ = 0;
  }
#endif  // USE_BINARY_SENSOR
}

void QesanListener::dump_config() {
  ESP_LOGCONFIG(TAG, "QESAN BLE Remote:");
  if (this->filter_by_mac_address_) {
    uint8_t mac[6] = {0};
    mac[0] = (uint8_t) (this->mac_address_ >> 40);
    mac[1] = (uint8_t) (this->mac_address_ >> 32);
    mac[2] = (uint8_t) (this->mac_address_ >> 24);
    mac[3] = (uint8_t) (this->mac_address_ >> 16);
    mac[4] = (uint8_t) (this->mac_address_ >> 8);
    mac[5] = (uint8_t) (this->mac_address_ >> 0);
    ESP_LOGCONFIG(TAG, "  MAC address: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
  if (this->filter_by_remote_address_) {
    ESP_LOGCONFIG(TAG, "  Remote address: 0x%04X", this->remote_address_);
  }
#ifdef USE_BINARY_SENSOR
  for (auto *binary_sensor : this->binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Button", binary_sensor);
    ESP_LOGCONFIG(TAG, "    Code: 0x%02X", binary_sensor->get_button_code());
  }
#endif  // USE_BINARY_SENSOR
}

bool QesanListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (this->filter_by_mac_address_ && (this->mac_address_ != device.address_uint64())) {
    return false;
  }

  const auto &manufacturer_datas = device.get_manufacturer_datas();
  if (!manufacturer_datas.empty()) {
    return false;
  }

  const auto &service_datas = device.get_service_datas();
  if (!service_datas.empty()) {
    return false;
  }

  const auto &service_uuids = device.get_service_uuids();
  if (service_uuids.size() != 6) {
    return false;
  }

  // Data is sent as 6 16-bit service uuids
  uint8_t data_bytes[12];
  for (uint8_t i = 0; i < 6; i++) {
    uint16_t uuid16 = service_uuids[i].get_uuid().uuid.uuid16;
    data_bytes[2 * i] = uuid16 & 0xff;
    data_bytes[2 * i + 1] = uuid16 >> 8;
  }

  // Decode XOR-masked data
  for (uint8_t i = 2; i < 12; i++) {
    data_bytes[i] ^= data_bytes[1];
  }

  uint16_t remote_address = (data_bytes[6] << 8) | data_bytes[7];

  if (this->filter_by_remote_address_ && (this->remote_address_ != remote_address)) {
    return false;
  }

  this->last_message_received_ = millis();

  if (this->last_xor_mask_ == data_bytes[1] && this->last_action_ == data_bytes[0] &&
      this->last_remote_address_ == remote_address) {
    // Duplicate received
    return false;
  }

  this->last_action_ = data_bytes[0];
  this->last_xor_mask_ = data_bytes[1];
  this->last_remote_address_ = remote_address;

  ESP_LOGVV(TAG, "Received raw message: %s", format_hex_pretty(data_bytes, 12).c_str());

  if ((data_bytes[2] != 0x00) || (data_bytes[3] != 0x01) || (data_bytes[4] != 0x02) || (data_bytes[5] != 0x02) ||
      (data_bytes[8] != 0xF9) || (data_bytes[9] != 0xF8) || (data_bytes[10] != 0xF7)) {
    ESP_LOGE(TAG, "Unexpected constants received (%02X %02X %02X %02X %02X %02X %02X)", data_bytes[2], data_bytes[3],
             data_bytes[4], data_bytes[5], data_bytes[8], data_bytes[9], data_bytes[10]);
    return false;
  }

  if ((data_bytes[0] & 0xF7) != 0) {
    ESP_LOGE(TAG, "Unknown action received (0x%02X)", data_bytes[0]);
    return false;
  }

  bool action = ((data_bytes[0] & 0x08) != 0);
  bool matched = false;

#ifdef USE_BINARY_SENSOR
  for (auto *binary_sensor : this->binary_sensors_) {
    matched |= binary_sensor->on_update_received(data_bytes[11], action);
  }
#endif  // USE_BINARY_SENSOR

  if (!matched) {
    ESP_LOGD(TAG, "[%s] Remote address: 0x%04X, Button code: 0x%02X %s", device.address_str().c_str(), remote_address,
             data_bytes[11], action ? "pressed" : "released");
  }

  return false;
}

}  // namespace qesan_ble_remote
}  // namespace esphome

#endif
