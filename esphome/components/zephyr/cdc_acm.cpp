#include "cdc_acm.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <zephyr/drivers/uart.h>
#ifndef CONFIG_USB_DEVICE_STACK_NEXT
#include <zephyr/drivers/uart/cdc_acm.h>
#endif

#define DEVICE_AND_COMMA(node_id) DEVICE_DT_GET(node_id),

namespace esphome::zephyr {

ESPHOME_LOG_TAG(TAG, "zephyr.cdc_acm");

static CdcAcm cdc_acm_instance;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
CdcAcm *const global_cdc_acm = &cdc_acm_instance;

#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
static const device *const CDC_DEVICES[] = {DT_FOREACH_STATUS_OKAY(zephyr_cdc_acm_uart, DEVICE_AND_COMMA)};
#endif

#ifdef CONFIG_USB_DEVICE_STACK_NEXT

// The new stack reports a rate change only with a USB device message, delivered on the system work queue.
// Handing it to the scheduler from there stopped that work queue, and with it all USB transfers, so the
// rates are read from the main loop instead. A rate the host sets and changes back within one poll is missed.
static constexpr uint32_t RATE_POLL_INTERVAL_MS = 500;

void CdcAcm::start_() {
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
  // Only changes after this point count, as on the legacy stack
  for (size_t i = 0; i < this->rates_.size(); i++)
    uart_line_ctrl_get(CDC_DEVICES[i], UART_LINE_CTRL_BAUD_RATE, &this->rates_[i]);
  App.scheduler.set_interval(static_cast<Component *>(nullptr), static_cast<const char *>(nullptr),
                             RATE_POLL_INTERVAL_MS, [this]() { this->poll_rates_(); });
#endif
}

void CdcAcm::poll_rates_() {
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
  for (size_t i = 0; i < this->rates_.size(); i++) {
    uint32_t rate = 0;
    if (uart_line_ctrl_get(CDC_DEVICES[i], UART_LINE_CTRL_BAUD_RATE, &rate) != 0 || rate == this->rates_[i])
      continue;
    this->rates_[i] = rate;
    ESP_LOGV(TAG, "%s: host set %" PRIu32 " baud", CDC_DEVICES[i]->name, rate);
    this->rate_callbacks_.call(CDC_DEVICES[i], rate);
  }
#endif
}

#else  // legacy USB device stack

void CdcAcm::start_() {
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
  for (const device *dev : CDC_DEVICES) {
    // only one global callback can be registered
    cdc_acm_dte_rate_callback_set(dev, CdcAcm::rate_changed_);
  }
#endif
}

void CdcAcm::rate_changed_(const struct device *device, uint32_t rate) {
  // Unnamed, so each rate change is queued in order instead of replacing a pending one
  App.scheduler.set_timeout(static_cast<Component *>(nullptr), static_cast<const char *>(nullptr), 0, [device, rate]() {
    ESP_LOGV(TAG, "%s: host set %" PRIu32 " baud", device->name, rate);
    global_cdc_acm->rate_callbacks_.call(device, rate);
  });
}

#endif

}  // namespace esphome::zephyr

#endif
