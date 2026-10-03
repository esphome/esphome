#include "uart_bridge.h"

#include "esphome/core/log.h"

namespace esphome::uart {

static const char *const TAG = "uart.bridge";

static constexpr size_t CHUNK = 64;

static void copy_bytes(UARTComponent *from, UARTComponent *to) {
  size_t waiting = from->available();
  if (waiting == 0) {
    return;
  }
  size_t room = to->available_for_write();
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

void UARTBridge::dump_config() { ESP_LOGCONFIG(TAG, "UART Bridge"); }

}  // namespace esphome::uart
