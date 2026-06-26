#include "esphome/core/defines.h"

#if defined(USE_ESP32) || defined(USE_LIBRETINY)

#include "bluetooth_proxy_advertisements.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::bluetooth_proxy {

static const char *const TAG = "bluetooth_proxy";

void BluetoothProxyAdvertisements::add_advertisement(uint64_t address, int rssi, uint8_t address_type,
                                                     const uint8_t *data, uint16_t data_len) {
  // Discard when no API client is subscribed (mirrors the ESP32 path, which only
  // appends while api_connection_ is set).
  if (this->api_connection_ == nullptr)
    return;

  // Defensive: never index past the fixed-size response array. The flush-on-full at the
  // end keeps advertisements_len below the batch size, but guard the index regardless.
  if (this->response_.advertisements_len >= BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE)
    this->flush_pending_advertisements_();

  auto &adv = this->response_.advertisements[this->response_.advertisements_len];
  // data[] is 62 bytes (legacy adv 31 + scan response 31); never copy past it.
  uint8_t copy_len =
      data_len <= sizeof(adv.data) ? static_cast<uint8_t>(data_len) : static_cast<uint8_t>(sizeof(adv.data));
  adv.address = address;
  adv.rssi = rssi;
  adv.address_type = address_type;
  adv.data_len = copy_len;
  std::memcpy(adv.data, data, copy_len);
  this->response_.advertisements_len++;

  // Flush once the batch is full (BATCH_SIZE entries fit the response array exactly).
  if (this->response_.advertisements_len >= BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE) {
    this->flush_pending_advertisements_();
  }
}

void BluetoothProxyAdvertisements::flush_pending_advertisements_() {
  if (this->response_.advertisements_len == 0)
    return;
  this->api_connection_->send_message(this->response_);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  this->log_advertisement_flush_();
#endif
  this->response_.advertisements_len = 0;
}

void BluetoothProxyAdvertisements::log_advertisement_flush_() {
  ESP_LOGV(TAG, "Sent batch of %u BLE advertisements", this->response_.advertisements_len);
}

}  // namespace esphome::bluetooth_proxy

#endif  // USE_ESP32 || USE_LIBRETINY
