#include "uart_bridge.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>

namespace esphome::uart {

static const char *const TAG = "uart.bridge";

static constexpr size_t CHUNK = 64;
// Bytes per 16 ms loop pass at 10 bits per byte: baud / 10 / 62.5.
static constexpr uint32_t BAUD_PACE_DIVISOR = 625;

static size_t write_room(UARTComponent *to) {
  size_t room = to->available_for_write();
  if (room == SIZE_MAX) {
    // Capacity unknown on this platform; pace to one loop pass of UART time
    // so a blocking write stays short.
    room = std::max<size_t>(1, to->get_baud_rate() / BAUD_PACE_DIVISOR);
  }
  return room;
}

static void copy_bytes(UARTComponent *from, UARTComponent *to) {
  size_t waiting = from->available();
  if (waiting == 0) {
    return;
  }
  size_t room = write_room(to);
  if (room == 0) {
    return;
  }
  if (waiting < room) {
    room = waiting;
  }
  if (room > CHUNK) {
    room = CHUNK;
  }
  uint8_t buf[CHUNK];
  if (!from->read_array(buf, room)) {
    return;
  }
  to->write_array(buf, room);
}

void UARTBridge::loop() {
  if (this->a_ == nullptr || this->b_ == nullptr) {
    return;
  }
  copy_bytes(this->a_, this->b_);
  copy_bytes(this->b_, this->a_);
}

void UARTBridge::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UART Bridge:\n"
                "  A: %" PRIu32 " baud\n"
                "  B: %" PRIu32 " baud",
                this->a_->get_baud_rate(), this->b_->get_baud_rate());
}

}  // namespace esphome::uart
