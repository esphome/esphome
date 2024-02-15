#include "ring_buffer.h"
#include <zephyr/drivers/uart.h>

#ifndef CONFIG_UART_INTERRUPT_DRIVEN
#error "CONFIG_UART_INTERRUPT_DRIVEN is required"
#endif

//   while (true) {
//     uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
//     if (dtr) {
//       break;
//     } else {
//       /* Give CPU resources to low priority threads. */
//       k_sleep(K_MSEC(100));
//     }
//   }

// }

namespace esphome {
namespace zephyr_uart {

void RingBuffer::setup(const device *dev, uint8_t *ring_buffer, uint32_t size) {
  dev_ = dev;
  ring_buf_init(&ringbuf_, size, ring_buffer);
  uart_irq_callback_user_data_set(dev, interrupt_handler_, this);
}

uint32_t RingBuffer::fill(const char *data, uint32_t size) {
  // TODO block if no space
  uint32_t rb_len = ring_buf_put(&ringbuf_, reinterpret_cast<const uint8_t*>(data), size);
  if (rb_len) {
    uart_irq_tx_enable(dev_);
  }
  return rb_len;
}

void RingBuffer::interrupt_handler_(const device *dev, void *user_data) {
  auto thiz = static_cast<RingBuffer *>(user_data);
  while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
    if (uart_irq_tx_ready(dev)) {
      uint8_t buffer[64];
      int rb_len, send_len;

      rb_len = ring_buf_get(&thiz->ringbuf_, buffer, sizeof(buffer));
      if (!rb_len) {
        uart_irq_tx_disable(dev);
        continue;
      }

      uart_fifo_fill(dev, buffer, rb_len);
      // TODO add blocking
    }
  }
}

}  // namespace zephyr_uart
}  // namespace esphome
