#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ZEPHYR_USB_BAUD_RATE
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <zephyr/usb/usbd.h>

namespace esphome::zephyr {

// Watches for baud rate changes on the USB serial port. Each time the USB host sets a new rate,
// the registered callbacks run on the main loop with that rate.
class UsbBaudRate final : public Component {
 public:
  UsbBaudRate();
  void setup() override;
  void loop() override;
  template<typename F> void add_on_baud_rate_callback(F &&callback) { this->callbacks_.add(std::forward<F>(callback)); }

  // Registered with the USB stack at boot, see usb_baud_rate.cpp.
  static void usbd_message_callback(usbd_context *const ctx, const usbd_msg *const msg);

 protected:
  // The USB thread queues rates here (it is the only writer) and the main loop drains them.
  static constexpr uint8_t RATE_QUEUE_SIZE = 4;
  volatile uint32_t rate_queue_[RATE_QUEUE_SIZE]{};
  volatile uint8_t queue_head_{0};
  volatile uint8_t queue_tail_{0};
  CallbackManager<void(uint32_t)> callbacks_;
};

}  // namespace esphome::zephyr

#endif
