#include "espnow_platform.h"

#ifdef USE_ESP32

#include "espnow_component.h"

#include <esp_idf_version.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <cstring>

namespace esphome::espnow::platform {

static void on_data_received(const esp_now_recv_info_t *info, const uint8_t *data, int size) {
  global_esp_now->packet_received(info->src_addr, info->des_addr, data, size, info->rx_ctrl->rssi,
                                  info->rx_ctrl->timestamp);
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
static void on_send_report(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  global_esp_now->send_reported(info->des_addr, status);
}
#else
static void on_send_report(const uint8_t *mac_addr, esp_now_send_status_t status) {
  global_esp_now->send_reported(mac_addr, status);
}
#endif

void init_radio() {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_disconnect());
}

void set_channel(uint8_t channel) {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
}

uint8_t get_channel() {
  uint8_t channel = 0;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&channel, &second);
  return channel;
}

void read_mac(uint8_t *mac) { esp_wifi_get_mac(WIFI_IF_STA, mac); }

esp_err_t init() {
  esp_err_t err = esp_now_init();
  if (err == ESP_OK) {
    err = esp_now_register_recv_cb(on_data_received);
  }
  if (err == ESP_OK) {
    err = esp_now_register_send_cb(on_send_report);
  }
  return err;
}

esp_err_t deinit() {
  esp_now_unregister_recv_cb();
  esp_now_unregister_send_cb();
  return esp_now_deinit();
}

uint32_t get_version() {
  uint32_t version = 0;
  esp_now_get_version(&version);
  return version;
}

bool peer_exists(const uint8_t *mac) { return esp_now_is_peer_exist(mac); }

esp_err_t add_peer(const uint8_t *mac) {
  esp_now_peer_info_t peer_info = {};
  peer_info.ifidx = WIFI_IF_STA;
  memcpy(peer_info.peer_addr, mac, ESP_NOW_ETH_ALEN);
  return esp_now_add_peer(&peer_info);
}

esp_err_t del_peer(const uint8_t *mac) { return esp_now_del_peer(mac); }

// Peers are added on channel 0, which ESP-IDF treats as whatever channel the radio is on
void rebind_peers() {}

esp_err_t send(const uint8_t *mac, const uint8_t *data, uint16_t size) { return esp_now_send(mac, data, size); }

}  // namespace esphome::espnow::platform

#endif  // USE_ESP32
