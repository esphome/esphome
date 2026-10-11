#include "usb_baud_rate.h"

#ifdef USE_ZEPHYR_USB_BAUD_RATE

#include "esphome/core/log.h"

#include <zephyr/drivers/uart.h>

namespace esphome::zephyr {

static const char *const TAG = "usb_baud_rate";

// Zephyr's USB event hook has no user data argument, so the callback finds the instance here.
static UsbBaudRate *instance{nullptr};   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool callback_registered{false};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

UsbBaudRate::UsbBaudRate() {
  if (instance == nullptr)
    instance = this;
}

void UsbBaudRate::setup() {
  if (instance != this) {
    ESP_LOGW(TAG, "Only one instance is supported");
    this->mark_failed();
  } else if (!callback_registered) {
    ESP_LOGW(TAG, "No USB device found");
    this->mark_failed();
  } else if (this->queue_tail_ == this->queue_head_) {
    this->disable_loop();  // woken by the USB thread when a rate arrives
  }
}

// Runs on the USB thread: queue the rate and wake the main loop, which runs the callbacks.
void UsbBaudRate::usbd_message_callback(usbd_context *const, const usbd_msg *const msg) {
  uint32_t rate;
  if (instance == nullptr || msg->type != USBD_MSG_CDC_ACM_LINE_CODING ||
      uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_BAUD_RATE, &rate) != 0)
    return;
  const uint8_t next = (instance->queue_head_ + 1) % RATE_QUEUE_SIZE;
  if (next == instance->queue_tail_)  // full: drop the newest
    return;
  instance->rate_queue_[instance->queue_head_] = rate;
  instance->queue_head_ = next;
  instance->enable_loop_soon_any_context();
}

void UsbBaudRate::loop() {
  while (this->queue_tail_ != this->queue_head_) {
    const uint32_t rate = this->rate_queue_[this->queue_tail_];
    this->queue_tail_ = (this->queue_tail_ + 1) % RATE_QUEUE_SIZE;
    this->callbacks_.call(rate);
  }
  this->disable_loop();
}

}  // namespace esphome::zephyr

// The USB stack refuses a new callback once the device is enabled, and the board enables it at the later init
// priority CONFIG_APPLICATION_INIT_PRIORITY, so register at priority 0. The board has one USB device.
// Global namespace: STRUCT_SECTION_FOREACH declares linker symbols.
static int register_usbd_message_callback() {
  STRUCT_SECTION_FOREACH(usbd_context, ctx) {
    if (usbd_msg_register_cb(ctx, esphome::zephyr::UsbBaudRate::usbd_message_callback) == 0) {
      esphome::zephyr::callback_registered = true;
      break;
    }
  }
  return 0;
}

SYS_INIT(register_usbd_message_callback, APPLICATION, 0);

#endif
