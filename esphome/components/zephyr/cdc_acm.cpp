#include "cdc_acm.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)
#include "esphome/core/application.h"
#include <zephyr/drivers/uart.h>
#ifndef CONFIG_USB_DEVICE_STACK_NEXT
#include <zephyr/drivers/uart/cdc_acm.h>
#endif

#define DEVICE_AND_COMMA(node_id) DEVICE_DT_GET(node_id),

namespace esphome::zephyr {

static CdcAcm cdc_acm_instance;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
CdcAcm *const global_cdc_acm = &cdc_acm_instance;

void CdcAcm::set_driver_callbacks_() {
  // The new USB device stack reports rate changes with a USB device message, which usb_device.cpp
  // forwards to rate_changed(), so only the legacy stack needs a callback per port
#if !defined(CONFIG_USB_DEVICE_STACK_NEXT) && DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
  const struct device *cdc_dev[] = {DT_FOREACH_STATUS_OKAY(zephyr_cdc_acm_uart, DEVICE_AND_COMMA)};
  for (auto &idx : cdc_dev) {
    // only one global callback can be registered
    cdc_acm_dte_rate_callback_set(idx, CdcAcm::rate_changed);
  }
#endif
}

void CdcAcm::rate_changed(const struct device *device, uint32_t rate) {
  // Unnamed, so each rate change is queued in order instead of replacing a pending one
  App.scheduler.set_timeout(static_cast<Component *>(nullptr), static_cast<const char *>(nullptr), 0,
                            [device, rate]() { global_cdc_acm->rate_callbacks_.call(device, rate); });
}

}  // namespace esphome::zephyr

#endif
