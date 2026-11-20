// Linker wrap for the esp_hosted 3.1.0 SDIO host. A co-processor older than 2.12.0 streams but does not say so in
// its init event, so the host fixes its RX staging buffers at one frame and then drops every larger read
// (esp-hosted-mcu#259). Skipping a one-frame target leaves the buffers growing on demand, as they did before 3.1.0.

#include "sdkconfig.h"

#ifdef CONFIG_ESP_HOSTED_HOST_TRANSPORT_BUS_SDIO

#include <cstdint>

namespace esphome::esp32_hosted {

// ESP_TRANSPORT_SDIO_MAX_BUF_SIZE
static constexpr uint32_t SDIO_FRAME_SIZE = 1536;

}  // namespace esphome::esp32_hosted

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

// Weak so a build that pins an esp_hosted before 3.1.0, which has no such function, still links
void __real_eh_host_bus_sdio_set_rx_staging_target(uint32_t target_size) __attribute__((weak));

void __wrap_eh_host_bus_sdio_set_rx_staging_target(uint32_t target_size) {
  if (target_size <= esphome::esp32_hosted::SDIO_FRAME_SIZE) {
    return;
  }
  __real_eh_host_bus_sdio_set_rx_staging_target(target_size);
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // CONFIG_ESP_HOSTED_HOST_TRANSPORT_BUS_SDIO
