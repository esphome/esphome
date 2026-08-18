#include "espnow_platform.h"

#if defined(USE_ESP8266)

extern "C" {
#include <user_interface.h>
}

namespace esphome::espnow::espnow_esp8266 {

void setup_network_stack() {}

void init_wifi_station() {
  wifi_set_opmode_current(STATION_MODE);
  wifi_set_sleep_type(NONE_SLEEP_T);
  wifi_station_disconnect();
}

espnow_err_t set_self_role() { return esp_now_set_self_role(ESP_NOW_ROLE_COMBO); }

void read_mac(uint8_t *own_address) { wifi_get_macaddr(STATION_IF, own_address); }

void apply_wifi_channel(uint8_t wifi_channel) {
  wifi_promiscuous_enable(true);
  wifi_set_channel(wifi_channel);
  wifi_promiscuous_enable(false);
}

uint8_t get_wifi_channel() { return wifi_get_channel(); }

espnow_err_t add_peer(const uint8_t *peer) {
  return esp_now_add_peer(const_cast<uint8_t *>(peer), ESP_NOW_ROLE_COMBO, 0, nullptr, 0);
}

espnow_err_t del_peer(const uint8_t *peer) { return esp_now_del_peer(const_cast<uint8_t *>(peer)); }

}  // namespace esphome::espnow::espnow_esp8266

#endif  // USE_ESP8266
