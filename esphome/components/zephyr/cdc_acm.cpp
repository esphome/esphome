#include "cdc_acm.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)
#include "esphome/core/lock_free_queue.h"
#include <atomic>
#include <zephyr/drivers/uart.h>
#ifndef CONFIG_USB_DEVICE_STACK_NEXT
#include <zephyr/drivers/uart/cdc_acm.h>
#endif

namespace esphome::zephyr {

CdcAcm *global_cdc_acm = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
// Ports whose host set a bit rate. Static, so rates set during boot are kept too.
LockFreeQueue<const device, 8> queue;
// Written once on the main loop, read on the USB driver's thread
std::atomic<CdcAcm *> instance{nullptr};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace

#ifndef CONFIG_USB_DEVICE_STACK_NEXT
#define DEVICE_AND_COMMA(node_id) DEVICE_DT_GET(node_id),
#endif

CdcAcm::CdcAcm() {
  global_cdc_acm = this;
  instance.store(this, std::memory_order_release);
}

void CdcAcm::setup() {
  // The new stack reports rates with a USB device message, which usb_device.cpp passes to rate_changed()
#if !defined(CONFIG_USB_DEVICE_STACK_NEXT) && DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
  for (const device *dev : {DT_FOREACH_STATUS_OKAY(zephyr_cdc_acm_uart, DEVICE_AND_COMMA)}) {
    // only one global callback can be registered
    cdc_acm_dte_rate_callback_set(dev, [](const device *port, uint32_t /*rate*/) { CdcAcm::rate_changed(port); });
  }
#endif
}

void CdcAcm::loop() {
  while (const device *dev = queue.pop()) {
    uint32_t rate;
    if (uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &rate) == 0)
      this->rate_callbacks_.call(dev, rate);
  }
  // A port queued after the last pop has already asked for the loop again
  this->disable_loop();
}

void CdcAcm::rate_changed(const device *device) {
  // Runs on the USB driver's thread: heavier work here, like a scheduler call, stalled USB on the new stack
  queue.push(device);
  if (CdcAcm *cdc_acm = instance.load(std::memory_order_acquire); cdc_acm != nullptr)
    cdc_acm->enable_loop_soon_any_context();
}

}  // namespace esphome::zephyr

#endif
