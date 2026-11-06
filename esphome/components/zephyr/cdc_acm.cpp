#include "cdc_acm.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)
#include "esphome/core/application.h"
#include <atomic>
#include <zephyr/drivers/uart.h>
#ifndef CONFIG_USB_DEVICE_STACK_NEXT
#include <zephyr/drivers/uart/cdc_acm.h>
#endif

namespace esphome::zephyr {

static CdcAcm cdc_acm_instance;              // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
CdcAcm *global_cdc_acm = &cdc_acm_instance;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace {
// USB starts in the logger's pre_setup(), before App is constructed, and the host sets a bit rate as soon as the
// device enumerates. App.scheduler must not be used before App exists, so rates are only posted once a component
// has asked for them in its setup(); start_() then reports the rates set until that point.
std::atomic<bool> started{false};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// 0 ms timeouts run on the main loop in the order they are posted
void post(std::function<void()> &&func) {
  App.scheduler.set_timeout(static_cast<Component *>(nullptr), static_cast<const char *>(nullptr), 0, std::move(func));
}
}  // namespace

void CdcAcm::start_() {
  // The new stack reports rates with a USB device message, which usb_device.cpp passes to rate_changed()
#ifndef CONFIG_USB_DEVICE_STACK_NEXT
  for (const device *dev : DEVICES) {
    // only one global callback can be registered
    cdc_acm_dte_rate_callback_set(dev, CdcAcm::rate_changed);
  }
#endif
  started.store(true, std::memory_order_release);
  post([]() {
    for (const device *dev : DEVICES) {
      uint32_t rate = 0;
      if (uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &rate) == 0)
        global_cdc_acm->report_rate_(dev, rate);
    }
  });
}

void CdcAcm::report_rate_(const device *dev, uint32_t rate) {
  for (size_t i = 0; i < DEVICES.size(); i++) {
    // The report of the current rates and a rate posted just before it can be the same change
    if (DEVICES[i] == dev && this->rates_[i] != rate) {
      this->rates_[i] = rate;
      this->rate_callbacks_.call(dev, rate);
    }
  }
}

void CdcAcm::rate_changed(const device *dev, uint32_t rate) {
  if (started.load(std::memory_order_acquire))
    post([dev, rate]() { global_cdc_acm->report_rate_(dev, rate); });
}

}  // namespace esphome::zephyr

#endif
