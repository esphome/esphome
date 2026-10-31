#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_ETHERNET_W5500)

#include <esp_idf_version.h>
// IDF 6.0 moved the per-chip SPI MAC drivers to the Espressif Component Registry; eth_w5500_config_t
// is no longer reachable through esp_eth.h and needs the explicit header.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include <esp_eth_mac_w5500.h>
#else
#include <esp_eth.h>
#endif

namespace esphome::ethernet {

// Installs a custom W5500 SPI driver that offloads the bulk frame transfers off the busy-wait path.
//
// The stock W5500 driver runs every SPI transfer through spi_device_polling_transmit(), which
// busy-waits the CPU for the whole transfer. The frame payload (one large read per received frame,
// one large write per transmitted frame) is by far the biggest transfer, so the RX task and the TX
// caller each spin for hundreds of microseconds per frame. This driver sends payload transfers
// through the blocking, interrupt-driven spi_device_transmit() instead, so the calling task sleeps
// while DMA moves the bytes. Small register accesses stay on the polling path, where the busy-wait
// is cheaper than an interrupt round-trip.
//
// Must be called before esp_eth_mac_new_w5500(). The driver reads spi_host_id and spi_devcfg back
// out of `config` in its init() callback, so `config` (and the spi_devcfg it points at) must stay
// alive until esp_eth_mac_new_w5500() returns.
void install_w5500_async_spi(eth_w5500_config_t &config);

}  // namespace esphome::ethernet

#endif  // USE_ESP32 && USE_ETHERNET_W5500
