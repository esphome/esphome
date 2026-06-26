#ifdef USE_LIBRETINY

// component.h first: it pulls in esphome/core/defines.h which sets USE_BLUETOOTH_PROXY
// before the guards in the headers below are evaluated.
#include "esphome/core/component.h"
#include "bluetooth_proxy_libretiny.h"

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::bluetooth_proxy {

static const char *const TAG = "bluetooth_proxy";

// Looked up by api_connection.cpp; assigned in setup().
BluetoothProxy *global_bluetooth_proxy = nullptr;  // NOLINT

void BluetoothProxy::setup() {
  // Defensive: ble_id (the tracker hub) is a required, codegen-resolved id, so hub_ should
  // never be null in a valid config — but guard rather than dereference it blindly. Without a
  // hub there is nothing to forward, so disable the component and leave global_bluetooth_proxy
  // null, which keeps subscribe/get_mac (which also use hub_) from ever being reached.
  if (this->hub_ == nullptr) {
    ESP_LOGE(TAG, "No BLE tracker hub attached; bluetooth_proxy disabled");
    this->mark_failed();
    return;
  }
  global_bluetooth_proxy = this;
  // Wire the hub's raw-advertisement stream into this proxy. The hub delivers on the main
  // loop (the same task that runs loop()), so the shared batching core stays single-threaded
  // — identical to the ESP32 proxy, where esp32_ble_tracker also delivers on the main loop.
  this->hub_->set_proxy_scan_result_callback(
      [this](const uint8_t *mac, int rssi, uint8_t addr_type, const uint8_t *data, uint16_t data_len) {
        this->on_advertisement(mac, rssi, addr_type, data, data_len);
      });
}

void BluetoothProxy::dump_config() {
  // Same lines/level as the ESP32 proxy. LibreTiny is advertisement-only, so active
  // connections are always off (Active: NO, Connections: 0).
  ESP_LOGCONFIG(TAG,
                "Bluetooth Proxy:\n"
                "  Active: %s\n"
                "  Connections: %d",
                "NO", 0);
}

void BluetoothProxy::get_bluetooth_mac_address_pretty(char output[18]) {
  uint8_t mac[6];
  this->hub_->get_proxy_mac(mac);  // hub returns printable (MSB-first) order
  snprintf(output, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void BluetoothProxy::subscribe_api_connection(api::APIConnection *conn, uint32_t /*flags*/) {
  // Same guard + message/level as the ESP32 proxy.
  if (this->api_connection_ != nullptr) {
    ESP_LOGE(TAG, "Only one API subscription is allowed at a time");
    return;
  }
  this->api_connection_ = conn;

  // Do not start scanning here — exactly like the ESP32 proxy. Scanning is driven by the
  // hub's scan_parameters and the user's api: start_scan automation. Report current state.
  api::BluetoothScannerStateResponse state_resp;
  state_resp.state = this->hub_->proxy_scan_running() ? api::enums::BLUETOOTH_SCANNER_STATE_RUNNING
                                                      : api::enums::BLUETOOTH_SCANNER_STATE_IDLE;
  state_resp.mode = this->hub_->proxy_scan_active() ? api::enums::BLUETOOTH_SCANNER_MODE_ACTIVE
                                                    : api::enums::BLUETOOTH_SCANNER_MODE_PASSIVE;
  state_resp.configured_mode = state_resp.mode;
  conn->send_message(state_resp);
}

void BluetoothProxy::unsubscribe_api_connection(api::APIConnection *conn) {
  // Same guard + message/level as the ESP32 proxy.
  if (this->api_connection_ != conn) {
    ESP_LOGV(TAG, "API connection is not subscribed");
    return;
  }
  this->api_connection_ = nullptr;
  // Discard the in-flight batch so it doesn't leak to the next subscriber.
  this->response_.advertisements_len = 0;
}

// Called by the tracker hub on the main loop for each raw advertisement — the same task that
// runs loop(), so the shared batching core (BluetoothProxyAdvertisements) stays single-threaded,
// exactly like the ESP32 proxy's parse_devices() path. The hub owns the BLE-task -> main-loop
// handoff, so no synchronization is needed here.
void BluetoothProxy::on_advertisement(const uint8_t *mac, int rssi, uint8_t addr_type, const uint8_t *data,
                                      uint16_t data_len) {
  // Only ingest while the API server is connected — mirrors the ESP32 path, where
  // parse_devices() pre-checks is_connected() before add_advertisement(). Without this,
  // a batch that fills between loop() ticks could flush (send_message) while disconnected.
  if (!api::global_api_server->is_connected() || this->api_connection_ == nullptr)
    return;

  // Assemble the address from the hub's bytes. The hub delivers mac[] least-significant-octet
  // first, so this produces the same uint64 layout as esp32_ble::ble_addr_to_uint64 (and the
  // tracker's ESPBTDevice::address_uint64), which Home Assistant decodes as the device MAC.
  uint64_t address = 0;
  for (int i = 0; i < 6; i++)
    address |= static_cast<uint64_t>(mac[i]) << (i * 8);

  // Same per-advertisement log as the ESP32 proxy (identical text + VERBOSE level); print the
  // MAC most-significant octet first (mac[5..0]) to match the human-readable order.
  ESP_LOGV(TAG, "Queuing raw packet from %02X:%02X:%02X:%02X:%02X:%02X, length %d. RSSI: %d dB", mac[5], mac[4], mac[3],
           mac[2], mac[1], mac[0], data_len, rssi);

  this->add_advertisement(address, rssi, addr_type, data, data_len);
}

void BluetoothProxy::loop() {
  // Flush any partially-filled advertisement batch every 100 ms — identical cadence to the
  // ESP32 proxy. add_advertisement() already flushes immediately once a batch fills.
  uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_advertisement_flush_time_ < 100)
    return;
  this->last_advertisement_flush_time_ = now;

  if (api::global_api_server->is_connected() && this->api_connection_ != nullptr)
    this->flush_pending_advertisements_();
}

}  // namespace esphome::bluetooth_proxy

#endif  // USE_LIBRETINY
