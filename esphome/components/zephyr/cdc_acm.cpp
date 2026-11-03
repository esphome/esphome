#include "cdc_acm.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)
#include "esphome/core/application.h"
#include <atomic>
#include <zephyr/drivers/uart.h>
#ifndef CONFIG_USB_DEVICE_STACK_NEXT
#include <zephyr/drivers/uart/cdc_acm.h>
#endif

#define DEVICE_AND_COMMA(node_id) DEVICE_DT_GET(node_id),

namespace esphome::zephyr {

static CdcAcm cdc_acm_instance;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
CdcAcm *const global_cdc_acm = &cdc_acm_instance;

namespace {
// USB starts in the logger's pre_setup(), before App is constructed, and the host sets a bit rate as soon as the
// device enumerates. App.scheduler must not be used before App exists, so rates are only posted once a component
// has asked for them in its setup(); start_() then reports the rates set until that point.
std::atomic<bool> started{false};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
const device *const CDC_DEVICES[] = {DT_FOREACH_STATUS_OKAY(zephyr_cdc_acm_uart, DEVICE_AND_COMMA)};
#endif
}  // namespace

void CdcAcm::start_() {
  // The new stack reports rates with a USB device message, which usb_device.cpp passes to rate_changed()
#if !defined(CONFIG_USB_DEVICE_STACK_NEXT) && DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
  for (const device *dev : CDC_DEVICES) {
    // only one global callback can be registered
    cdc_acm_dte_rate_callback_set(dev, [](const device *, uint32_t) { CdcAcm::rate_changed(); });
  }
#endif
  started.store(true, std::memory_order_release);
  // Runs after every component's setup(), so all callbacks are registered by then
  rate_changed();
}

void CdcAcm::report_rates_() {
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
  for (size_t i = 0; i < this->rates_.size(); i++) {
    uint32_t rate;
    if (uart_line_ctrl_get(CDC_DEVICES[i], UART_LINE_CTRL_BAUD_RATE, &rate) != 0 || rate == this->rates_[i])
      continue;
    this->rates_[i] = rate;
    this->rate_callbacks_.call(CDC_DEVICES[i], rate);
  }
#endif
}

void CdcAcm::rate_changed() {
  if (!started.load(std::memory_order_acquire))
    return;
  App.scheduler.set_timeout(static_cast<Component *>(nullptr), static_cast<const char *>(nullptr), 0,
                            []() { global_cdc_acm->report_rates_(); });
}

}  // namespace esphome::zephyr

#endif
