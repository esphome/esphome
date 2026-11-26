#include "cdc_acm.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)
#include "esphome/core/application.h"
#include <array>
#include <zephyr/drivers/uart.h>
#ifndef CONFIG_USB_DEVICE_STACK_NEXT
#include <zephyr/drivers/uart/cdc_acm.h>
#endif

#define DEVICE_AND_COMMA(node_id) DEVICE_DT_GET(node_id),

namespace esphome::zephyr {

namespace {
constexpr std::array<const device *, DT_NUM_INST_STATUS_OKAY(zephyr_cdc_acm_uart)> CDC_DEVICES{
    DT_FOREACH_STATUS_OKAY(zephyr_cdc_acm_uart, DEVICE_AND_COMMA)};

// USB starts in the logger's pre_setup(), before App exists, so nothing is posted until start_(). Reading a rate
// and posting it under one lock keeps the posts in the order the rates were read. The legacy stack reports a rate
// as the host sets it; the new stack queues a message without the rate, so when the host changes the rate again
// before that message is delivered, only the latest rate is posted.
Mutex rate_lock;       // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
bool started = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace

void CdcAcm::start() {
  LockGuard guard(rate_lock);
  started = true;
  for (const device *dev : CDC_DEVICES) {
#ifndef CONFIG_USB_DEVICE_STACK_NEXT
    // The new stack reports rates with a USB device message, which usb_device.cpp passes to rate_changed()
    cdc_acm_dte_rate_callback_set(dev, [](const device *dev, uint32_t) { CdcAcm::rate_changed(dev); });
#endif
    post_rate(dev);
  }
}

void CdcAcm::rate_changed(const device *dev) {
  LockGuard guard(rate_lock);
  if (started)
    post_rate(dev);
}

void CdcAcm::post_rate(const device *dev) {
  uint32_t rate = 0;
  if (uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &rate) != 0)
    return;
  // 0 ms timeouts run on the main loop in the order they are posted
  App.scheduler.set_timeout(static_cast<Component *>(nullptr), static_cast<const char *>(nullptr), 0,
                            [dev, rate]() { rate_callbacks.call(dev, rate); });
}

}  // namespace esphome::zephyr

#endif
