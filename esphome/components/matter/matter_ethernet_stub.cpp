#include "esphome/core/defines.h"

// USE_MATTER_VARIANT_SUPPORTED is set by matter's Python to_code() via
// cg.add_define() on the 5 esp-matter-supported ESP32 variants (ESP32,
// S3, C3, C6, H2). It is deliberately NOT declared in
// esphome/core/defines.h — that path is only exercised by clang-tidy and
// static-analysis tools, which do not have esp_matter.h available (the
// SDK is a third-party managed component fetched at build time). Keeping
// the symbol out of defines.h means matter files strip on lint (no
// missing-header errors) but compile normally on real builds where
// Python-side codegen has run. Runtime variant enforcement lives in the
// only_on_variant validator in matter/__init__.py.
#if defined(USE_ESP_IDF) && defined(USE_MATTER_VARIANT_SUPPORTED)
#ifdef USE_ETHERNET

// Override for esp-matter's default Ethernet network commissioning driver.
//
// esp-matter's own NetworkCommissioningDriver_Ethernet.cpp hardcodes PHY = IP101
// and calls esp_eth_driver_install() on its own, then esp_netif_attach() and
// esp_eth_start(). In an ESPHome build the `ethernet:` component already
// installed the PHY driver (whatever the user configured — LAN8720, W5500,
// etc.), created the netif, and started it — so esp-matter's Init() aborts
// with ESP_ERR_INVALID_ARG on esp_eth_driver_install(): a second install is
// not legal, and the PHY types don't match.
//
// esp-matter's own comment invites this override:
//   /* Currently default ethernet board supported is IP101, if you want to
//    * use other types of ethernet board then you can override this function
//    * in your application. */
//
// On every ESP32 variant, external_platform/external_platform.cmake
// excludes the upstream NetworkCommissioningDriver_Ethernet.cpp from the
// esp_matter component build (via CONFIG_CHIP_ENABLE_EXTERNAL_PLATFORM).
// That leaves ESPEthernetDriver::Init undefined in the esp-matter library
// — and this TU is its sole provider, compiled whenever ESPHome's
// ethernet: component is present (USE_ETHERNET). The earlier setup
// (exclusion only on non-EMAC variants; rely on link-order on classic
// ESP32) worked because the upstream TU only exports one symbol, but a
// future release adding a second external symbol could pull the archive
// object and blow up multiple-definition on classic ESP32. Uniform
// exclusion removes that hazard.

#include <platform/NetworkCommissioning.h>
#include <platform/ESP32/NetworkCommissioningDriver.h>

#include "esphome/core/log.h"

namespace esphome::matter {

static const char *const TAG = "matter.eth.stub";

// Emits a one-line log entry when esp-matter's Ethernet NetworkCommissioning
// driver Init runs — placed in the ESPHome matter namespace so the file
// carries at least one ESPHome-side symbol (integration convention). Called
// from the chip:: namespace override below.
void log_ethernet_driver_stub_init() {
  ESP_LOGI(TAG, "ESPEthernetDriver::Init override — ESPHome ethernet: owns the PHY, no-op");
}

}  // namespace esphome::matter

namespace chip {
namespace DeviceLayer {
namespace NetworkCommissioning {

CHIP_ERROR ESPEthernetDriver::Init(NetworkStatusChangeCallback * /*networkStatusChangeCallback*/) {
  esphome::matter::log_ethernet_driver_stub_init();
  return CHIP_NO_ERROR;
}

}  // namespace NetworkCommissioning
}  // namespace DeviceLayer
}  // namespace chip

#endif  // USE_ETHERNET
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
