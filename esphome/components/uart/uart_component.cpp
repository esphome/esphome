#include "uart_component.h"
#include "esphome/core/application.h"

#include <algorithm>

namespace esphome::uart {

ESPHOME_LOG_TAG(TAG, "uart");

// At 1 baud a byte of 10 bits takes 10 s.
static constexpr uint64_t BYTE_US_AT_ONE_BAUD = 10000000;

bool UARTComponent::check_read_timeout_(size_t len) {
  if (this->available() >= len)
    return true;

  uint32_t start_time = millis();
  while (this->available() < len) {
    if (millis() - start_time > 100) {
      ESP_LOGE(TAG, "Reading from UART timed out at byte %zu!", this->available());
      return false;
    }
    yield();
  }
  return true;
}

size_t UARTComponent::paced_write_room(uint32_t last_write_us) {
  size_t room = this->available_for_write();
  if (room != SIZE_MAX) {
    return room;
  }
  this->paces_writes_ = true;
  return this->paced_room_(micros() - last_write_us);
}

size_t UARTComponent::paced_room_(uint32_t span_us) const {
  // What the line sent, in ten-millionths of a byte. A pass woken early writes little. The cap of one loop interval
  // has a byte on top, so it never cuts the part of a byte that last_write_us carries over.
  uint64_t sent = std::min<uint64_t>(static_cast<uint64_t>(span_us) * this->baud_rate_,
                                     App.get_loop_interval() * 1000ULL * this->baud_rate_ + BYTE_US_AT_ONE_BAUD);
  return static_cast<size_t>(std::max<uint64_t>(1, sent / BYTE_US_AT_ONE_BAUD));
}

uint32_t UARTComponent::paced_stamp_(uint32_t now_us, uint32_t last_write_us) const {
  if (this->baud_rate_ == 0) {
    return now_us;
  }
  // Move on by whole bytes only, so the part of a byte the line sent after the last whole one counts for the next
  // write.
  uint64_t part = static_cast<uint64_t>(now_us - last_write_us) * this->baud_rate_ % BYTE_US_AT_ONE_BAUD;
  return now_us - static_cast<uint32_t>(part / this->baud_rate_);
}

void UARTComponent::set_rx_full_threshold_ms(uint8_t time) {
  uint8_t bytelength = this->data_bits_ + this->stop_bits_ + 1;
  if (this->parity_ != UARTParityOptions::UART_CONFIG_PARITY_NONE)
    bytelength += 1;
  int32_t val = clamp<int32_t>((this->baud_rate_ / (bytelength * 1000 / time)) - 1, 1, 120);
  this->set_rx_full_threshold(val);
}

#ifdef USE_ESP8266
void UARTComponent::write_array_progmem(const uint8_t *data, size_t len) {
  uint8_t buf[32];
  while (len > 0) {
    const size_t chunk = std::min(len, sizeof(buf));
    progmem_memcpy(buf, data, chunk);
    this->write_array(buf, chunk);
    data += chunk;
    len -= chunk;
  }
}
#endif

}  // namespace esphome::uart
