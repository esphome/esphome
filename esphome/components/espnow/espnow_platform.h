#pragma once

#if defined(USE_ESP8266)

#include "espnow_compat.h"

#include <cstdint>

namespace esphome::espnow::espnow_esp8266 {

void setup_network_stack();
void init_wifi_station();
espnow_err_t set_self_role();
void read_mac(uint8_t *own_address);
void apply_wifi_channel(uint8_t wifi_channel);
uint8_t get_wifi_channel();
espnow_err_t add_peer(const uint8_t *peer);
espnow_err_t del_peer(const uint8_t *peer);

}  // namespace esphome::espnow::espnow_esp8266

#endif  // USE_ESP8266
