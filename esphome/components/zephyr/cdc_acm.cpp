#include "cdc_acm.h"
#if defined(USE_ZEPHYR_CDC_ACM_RATE_CALLBACK)
#include <array>
#include <atomic>
#ifndef CONFIG_USB_DEVICE_STACK_NEXT
#include <zephyr/drivers/uart/cdc_acm.h>
#endif

namespace esphome::zephyr {

CdcAcm *global_cdc_acm = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace {

struct RateEvent {
  const device *dev;
  uint32_t rate;
};

// The USB driver's thread pushes, the main loop pops. Static, so rates set during boot are kept too.
// A host sets a rate when it opens a port, so a few slots are plenty.
static constexpr uint8_t QUEUE_SIZE = 8;
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
std::array<RateEvent, QUEUE_SIZE> queue;
std::atomic<uint8_t> queue_head{0};  // next slot to pop, written by the main loop
std::atomic<uint8_t> queue_tail{0};  // next slot to push, written by the USB driver's thread
// Written once on the main loop, read on the USB driver's thread
std::atomic<CdcAcm *> instance{nullptr};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

bool pop(RateEvent &event) {
  const uint8_t head = queue_head.load(std::memory_order_relaxed);
  if (head == queue_tail.load(std::memory_order_acquire))
    return false;
  event = queue[head];
  queue_head.store((head + 1) % QUEUE_SIZE, std::memory_order_release);
  return true;
}

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
    cdc_acm_dte_rate_callback_set(dev, CdcAcm::rate_changed);
  }
#endif
}

void CdcAcm::loop() {
  RateEvent event;
  while (pop(event))
    this->rate_callbacks_.call(event.dev, event.rate);
  // A rate queued after the last pop has already asked for the loop again
  this->disable_loop();
}

void CdcAcm::rate_changed(const device *device, uint32_t rate) {
  // Runs on the USB driver's thread: heavier work here, like a scheduler call, stalled USB on the new stack
  const uint8_t tail = queue_tail.load(std::memory_order_relaxed);
  const uint8_t next = (tail + 1) % QUEUE_SIZE;
  if (next == queue_head.load(std::memory_order_acquire))
    return;
  queue[tail] = {device, rate};
  queue_tail.store(next, std::memory_order_release);
  if (CdcAcm *cdc_acm = instance.load(std::memory_order_acquire); cdc_acm != nullptr)
    cdc_acm->enable_loop_soon_any_context();
}

}  // namespace esphome::zephyr

#endif
