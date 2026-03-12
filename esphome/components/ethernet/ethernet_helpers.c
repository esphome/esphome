#include "esp_eth_mac_esp.h"

// ETH_ESP32_EMAC_DEFAULT_CONFIG() uses out-of-order designated initializers
// which are valid in C but not in C++. This wrapper allows C++ code to get
// the default config without replicating the macro's contents.
eth_esp32_emac_config_t eth_esp32_emac_default_config(void) {
  return (eth_esp32_emac_config_t) ETH_ESP32_EMAC_DEFAULT_CONFIG();
}
