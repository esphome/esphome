// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "usb_host.h"
#include <cinttypes>
#include "esphome/core/log.h"

namespace esphome::usb_host {

void USBHost::setup() {
  usb_host_config_t config{};
#ifdef USB_HOST_RX_FIFO_LINES
  config.fifo_settings_custom.rx_fifo_lines = USB_HOST_RX_FIFO_LINES;
  config.fifo_settings_custom.nptx_fifo_lines = USB_HOST_NPTX_FIFO_LINES;
  config.fifo_settings_custom.ptx_fifo_lines = USB_HOST_PTX_FIFO_LINES;
#endif  // USB_HOST_RX_FIFO_LINES

  const esp_err_t err = usb_host_install(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "usb_host_install failed: %s", esp_err_to_name(err));
    this->status_set_error(LOG_STR("usb_host_install failed"));
    this->mark_failed();
    return;
  }
}

#ifdef USB_HOST_RX_FIFO_LINES
void USBHost::dump_config() {
  ESP_LOGCONFIG(TAG, "USB Host:");
  ESP_LOGCONFIG(TAG, "  FIFO lines: RX=%" PRIu32 ", NPTX=%" PRIu32 ", PTX=%" PRIu32,
                static_cast<uint32_t>(USB_HOST_RX_FIFO_LINES), static_cast<uint32_t>(USB_HOST_NPTX_FIFO_LINES),
                static_cast<uint32_t>(USB_HOST_PTX_FIFO_LINES));
}
#endif  // USB_HOST_RX_FIFO_LINES

void USBHost::loop() {
  int err;
  uint32_t event_flags;
  err = usb_host_lib_handle_events(0, &event_flags);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGD(TAG, "lib_handle_events failed failed: %s", esp_err_to_name(err));
  }
  if (event_flags != 0) {
    ESP_LOGD(TAG, "Event flags %" PRIu32 "X", event_flags);
  }
}

}  // namespace esphome::usb_host

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
