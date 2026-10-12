#pragma once

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include "espnow_types.h"

#include <cstdint>

// The radio and ESP-NOW driver calls that differ between ESP-IDF and the ESP8266 NONOS SDK. Each platform
// implements the whole set in its own source file, so the component itself carries no platform checks.
namespace esphome::espnow::platform {

// Bring up the station interface when no wifi component owns the radio
void init_radio();
void set_channel(uint8_t channel);
uint8_t get_channel();
void read_mac(uint8_t *mac);

// Start the driver and route its callbacks to the component
esp_err_t init();
esp_err_t deinit();
uint32_t get_version();  // 0 where the SDK has no version call

bool peer_exists(const uint8_t *mac);
esp_err_t add_peer(const uint8_t *mac);
esp_err_t del_peer(const uint8_t *mac);
// Move the registered peers to the radio's current channel; only the NONOS SDK binds peers to a channel
void rebind_peers();

esp_err_t send(const uint8_t *mac, const uint8_t *data, uint16_t size);

}  // namespace esphome::espnow::platform

#endif  // USE_ESP32 || USE_ESP8266
