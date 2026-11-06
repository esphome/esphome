#include "cdc_acm.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)
#include "esphome/core/application.h"
#include <atomic>
#include <iterator>
#include <zephyr/drivers/uart.h>
#ifndef CONFIG_USB_DEVICE_STACK_NEXT
#include <zephyr/drivers/uart/cdc_acm.h>
#endif

#define DEVICE_AND_COMMA(node_id) DEVICE_DT_GET(node_id),

namespace esphome::zephyr {

static CdcAcm cdc_acm_instance;              // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
CdcAcm *global_cdc_acm = &cdc_acm_instance;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

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
    cdc_acm_dte_rate_callback_set(dev, CdcAcm::rate_changed);
  }
#endif
  started.store(true, std::memory_order_release);
  // Runs on the next scheduler pass; add_on_rate_callback() passes the rates found to later callbacks
  App.scheduler.set_timeout(static_cast<Component *>(nullptr), static_cast<const char *>(nullptr), 0,
                            []() { global_cdc_acm->report_current_rates_(); });
}

const device *CdcAcm::device_at(size_t index) {
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
  return CDC_DEVICES[index];
#else
  return nullptr;
#endif
}

void CdcAcm::report_current_rates_() {
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
  for (const device *dev : CDC_DEVICES) {
    uint32_t rate = 0;
    if (uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &rate) == 0)
      this->report_rate_(dev, rate);
  }
#endif
}

void CdcAcm::report_rate_(const device *dev, uint32_t rate) {
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
  static_assert(std::size(CDC_DEVICES) == std::tuple_size_v<decltype(rates_)>);
  for (size_t i = 0; i < std::size(CDC_DEVICES); i++) {
    if (CDC_DEVICES[i] != dev)
      continue;
    // The first report of the current rates and a rate posted before it can be the same change
    if (rate != this->rates_[i]) {
      this->rates_[i] = rate;
      this->rate_callbacks_.call(dev, rate);
    }
    return;
  }
#endif
}

void CdcAcm::rate_changed(const device *dev, uint32_t rate) {
  if (!started.load(std::memory_order_acquire))
    return;
  // 0 ms timeouts run in the order they are posted
  App.scheduler.set_timeout(static_cast<Component *>(nullptr), static_cast<const char *>(nullptr), 0,
                            [dev, rate]() { global_cdc_acm->report_rate_(dev, rate); });
}

}  // namespace esphome::zephyr

#endif
