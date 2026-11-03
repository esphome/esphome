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

CdcAcm *global_cdc_acm = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace {
// Written once on the main loop, read on the USB driver's thread
std::atomic<CdcAcm *> instance{nullptr};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
const device *const CDC_DEVICES[] = {DT_FOREACH_STATUS_OKAY(zephyr_cdc_acm_uart, DEVICE_AND_COMMA)};
#endif
}  // namespace

CdcAcm::CdcAcm() {
  global_cdc_acm = this;
  instance.store(this, std::memory_order_release);
}

void CdcAcm::setup() {
  // The new stack reports rates with a USB device message, which usb_device.cpp passes to rate_changed()
#if !defined(CONFIG_USB_DEVICE_STACK_NEXT) && DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
  for (const device *dev : CDC_DEVICES) {
    // only one global callback can be registered
    cdc_acm_dte_rate_callback_set(dev, [](const device *, uint32_t) { CdcAcm::rate_changed(); });
  }
#endif
  // Report rates the host set during boot, once every component has registered its callback
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
  // TEST: post to the main loop with the scheduler from the USB driver's thread
  App.scheduler.set_timeout(static_cast<Component *>(nullptr), static_cast<const char *>(nullptr), 0, []() {
    if (CdcAcm *cdc_acm = instance.load(std::memory_order_acquire); cdc_acm != nullptr)
      cdc_acm->report_rates_();
  });
}

}  // namespace esphome::zephyr

#endif
