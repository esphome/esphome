#include "esphome/core/defines.h"

// esp-matter 1.6.0 only supports these ESP32 variants. Strip the whole
// TU on any other target (P4, S2, C2, C5, C61, H4, H21, S31) so clang-tidy
// jobs for those variants — which grep this file in via USE_WIFI /
// USE_ETHERNET — don't try to compile against an esp_matter.h that upstream
// never ships for those chips. Runtime builds are already rejected by the
// only_on_variant config validator in matter/__init__.py; this guard is the
// static-analysis mirror of the same restriction.
#ifdef USE_ESP_IDF
#if defined(USE_ESP32_VARIANT_ESP32) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32C3) || \
    defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32H2)
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
// On every ESP32 variant, PATCH1 in components/matter/_apply_patches.py
// overwrites the upstream NetworkCommissioningDriver_Ethernet.cpp with an
// empty stub before ninja compiles. That leaves ESPEthernetDriver::Init
// undefined in the esp-matter library — and this TU is its sole provider,
// compiled whenever ESPHome's ethernet: component is present (USE_ETHERNET).
// The earlier setup (patch only on non-EMAC variants; rely on link-order
// on classic ESP32) worked because the upstream TU only exports one
// symbol, but a future release adding a second external symbol could pull
// the archive object and blow up multiple-definition on classic ESP32.
// Uniform patch removes that hazard.

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
#endif  // matter supported variant
#endif  // USE_ESP_IDF
