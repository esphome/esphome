#pragma once
#include <zephyr/sys/ring_buffer.h>

class device;

namespace esphome {
namespace zephyr_uart {

class RingBuffer {
 public:
  void setup(const device *dev, uint8_t *ring_buffer, uint32_t size);
  uint32_t fill(const char* data, uint32_t size);
 protected:
  static void interrupt_handler_(const device *dev, void *user_data);
  ring_buf ringbuf_;
  const device *dev_ = {nullptr};
};

}  // namespace zephyr_uart
}  // namespace esphome
