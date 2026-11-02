#include "espnow_platform.h"

#ifdef USE_ESP8266

#include "espnow_component.h"

namespace esphome::espnow::platform {

// The SDK reports 0 for success and a non-zero value for any failure
static esp_err_t sdk_result(int result) { return result == 0 ? ESP_OK : ESP_FAIL; }

// Stands in for the destination address, which the SDK does not report
static const uint8_t UNKNOWN_ADDR[ESP_NOW_ETH_ALEN] = {0};

static void on_data_received(uint8_t *mac_addr, uint8_t *data, uint8_t size) {
  global_esp_now->packet_received(mac_addr, UNKNOWN_ADDR, data, size, 0, 0);
}

static void on_send_report(uint8_t *mac_addr, uint8_t status) {
  global_esp_now->send_reported(mac_addr, status == 0 ? ESP_NOW_SEND_SUCCESS : ESP_NOW_SEND_FAIL);
}

void init_radio() {
  // The NONOS SDK only delivers broadcast frames to ESP-NOW while the station is associated or the
  // soft-AP interface is up. Without Wi-Fi, run a hidden, open soft-AP that accepts no clients so
  // broadcasts (for example from a WizMote style remote) are received.
  softap_config ap{};
  ap.channel = 1;
  ap.authmode = AUTH_OPEN;
  ap.ssid_hidden = 1;
  ap.max_connection = 0;
  ap.beacon_interval = 60000;
  wifi_set_opmode_current(STATIONAP_MODE);
  wifi_softap_set_config_current(&ap);
  wifi_softap_dhcps_stop();
  wifi_set_sleep_type(NONE_SLEEP_T);
  wifi_station_disconnect();
}

void set_channel(uint8_t channel) {
  wifi_promiscuous_enable(true);
  wifi_set_channel(channel);
  wifi_promiscuous_enable(false);
  // Keep the soft-AP from init_radio() on the same channel so the SDK never retunes the radio to its own
  softap_config ap{};
  if (wifi_get_opmode() == STATIONAP_MODE && wifi_softap_get_config(&ap) && ap.channel != channel) {
    ap.channel = channel;
    wifi_softap_set_config_current(&ap);
  }
}

uint8_t get_channel() { return wifi_get_channel(); }

void read_mac(uint8_t *mac) {
  // Frames leave from the station interface unless the device runs as a soft-AP only
  wifi_get_macaddr(wifi_get_opmode() == SOFTAP_MODE ? SOFTAP_IF : STATION_IF, mac);
}

esp_err_t init() {
  int result = esp_now_init();
  if (result == 0) {
    // CONTROLLER sends from the station interface; the other roles send from the soft-AP MAC whenever
    // that interface is up, so peers would no longer recognise the sender.
    result = esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  }
  if (result == 0) {
    result = esp_now_register_recv_cb(on_data_received);
  }
  if (result == 0) {
    result = esp_now_register_send_cb(on_send_report);
  }
  return sdk_result(result);
}

esp_err_t deinit() {
  esp_now_unregister_recv_cb();
  esp_now_unregister_send_cb();
  return sdk_result(esp_now_deinit());
}

uint32_t get_version() { return 0; }

// The SDK returns 1 when the peer exists, 0 when it does not and a negative value on error
bool peer_exists(const uint8_t *mac) { return esp_now_is_peer_exist(const_cast<uint8_t *>(mac)) > 0; }

esp_err_t add_peer(const uint8_t *mac) {
  return sdk_result(esp_now_add_peer(const_cast<uint8_t *>(mac), ESP_NOW_ROLE_COMBO, wifi_get_channel(), nullptr, 0));
}

esp_err_t del_peer(const uint8_t *mac) { return sdk_result(esp_now_del_peer(const_cast<uint8_t *>(mac))); }

void rebind_peers() {
  const uint8_t channel = wifi_get_channel();
  for (uint8_t *mac = esp_now_fetch_peer(true); mac != nullptr; mac = esp_now_fetch_peer(false)) {
    esp_now_set_peer_channel(mac, channel);
  }
}

esp_err_t send(const uint8_t *mac, const uint8_t *data, uint16_t size) {
  return sdk_result(esp_now_send(const_cast<uint8_t *>(mac), const_cast<uint8_t *>(data), size));
}

}  // namespace esphome::espnow::platform

#endif  // USE_ESP8266
