#include "uart_component.h"
#include "esphome/core/application.h"

#include <algorithm>

namespace esphome::uart {

ESPHOME_LOG_TAG(TAG, "uart");

// Bits on the line per byte where the framing is not set, as on USB CDC
static constexpr uint8_t DEFAULT_FRAME_BITS = 10;

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
  return this->paced_room_(micros() - last_write_us);
}

uint8_t UARTComponent::frame_bits_() const {
  if (this->data_bits_ == 0)
    return DEFAULT_FRAME_BITS;
  // usb_uart stores 1.5 stop bits as 3; count it as 2, which errs towards less room
  return 1 + this->data_bits_ + std::min<uint8_t>(this->stop_bits_, 2) +
         (this->parity_ != UART_CONFIG_PARITY_NONE ? 1 : 0);
}

size_t UARTComponent::paced_room_(uint32_t span_us) const {
  // Without a baud rate there is no line time to pace by: one byte per pass
  if (this->baud_rate_ == 0)
    return 1;
  // In millionths of a bit, so a byte is frame_bits_() million. The cap has a byte on top so it never cuts the part
  // of a byte carried over. Less than a byte gives 0: a byte forced through would never be charged.
  const uint32_t byte_units = this->frame_bits_() * 1000000U;
  uint64_t sent = std::min<uint64_t>(static_cast<uint64_t>(span_us) * this->baud_rate_,
                                     App.get_loop_interval() * 1000ULL * this->baud_rate_ + byte_units);
  return static_cast<size_t>(sent / byte_units);
}

uint32_t UARTComponent::paced_stamp_(uint32_t now_us, uint32_t last_write_us) const {
  if (this->baud_rate_ == 0) {
    return now_us;
  }
  // Below one byte, so the divide is 32-bit
  const auto part = static_cast<uint32_t>(static_cast<uint64_t>(now_us - last_write_us) * this->baud_rate_ %
                                          (this->frame_bits_() * 1000000U));
  return now_us - part / this->baud_rate_;
}

void UARTComponent::set_rx_full_threshold_ms(uint8_t time) {
  const uint8_t bytelength = this->frame_bits_();
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
