// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "usb_host.h"
#include <cinttypes>
#include "esphome/core/log.h"
#if defined(USE_USB_HOST_FULL_SPEED_ONLY) && defined(USE_ESP32_VARIANT_ESP32P4)
#include "soc/usb_dwc_struct.h"
#endif

namespace esphome::usb_host {

void USBHost::setup() {
  usb_host_config_t config{};
#ifdef USE_USB_HOST_FIFO_CONFIG
  // Sized in Python from component requests
  config.fifo_settings_custom.rx_fifo_lines = USB_HOST_FIFO_RX_LINES;
  config.fifo_settings_custom.nptx_fifo_lines = USB_HOST_FIFO_NPTX_LINES;
  config.fifo_settings_custom.ptx_fifo_lines = USB_HOST_FIFO_PTX_LINES;
#endif

  if (usb_host_install(&config) != ESP_OK) {
    this->status_set_error(LOG_STR("usb_host_install failed"));
    this->mark_failed();
    return;
  }
#if defined(USE_USB_HOST_FULL_SPEED_ONLY) && defined(USE_ESP32_VARIANT_ESP32P4)
  this->hold_full_speed_();
#endif
}

#if defined(USE_USB_HOST_FULL_SPEED_ONLY) && defined(USE_ESP32_VARIANT_ESP32P4)
// espressif/usb 1.6 has usb_host_config_t.fsls_only, but it needs USB DWC HAL support that ESP-IDF 6.1 does not
// have and refuses the option there. HCFG.FSLSSupp is only cleared by a core soft reset (at install and when the
// root port recovers), so it is set after install and again whenever it reads back clear. Returns true when it was
// clear.
bool USBHost::hold_full_speed_() {
  if (USB_DWC_HS.hcfg_reg.fslssupp)
    return false;
  USB_DWC_HS.hcfg_reg.fslssupp = 1;
  return true;
}

void USBHost::dump_config() { ESP_LOGCONFIG(TAG, "USB Host:\n  Full speed only: YES"); }
#endif

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
#if defined(USE_USB_HOST_FULL_SPEED_ONLY) && defined(USE_ESP32_VARIANT_ESP32P4)
  // A port recovery runs inside usb_host_lib_handle_events() and soft resets the core
  if (this->hold_full_speed_()) {
    ESP_LOGI(TAG, "Root port reset; full speed only set again");
  }
#endif
}

}  // namespace esphome::usb_host

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
