#include "uart_component.h"
#include "esphome/core/application.h"

#include <algorithm>

namespace esphome::uart {

ESPHOME_LOG_TAG(TAG, "uart");

// Keeps the pacing product in 32 bits up to about 10 Mbaud.
static constexpr uint32_t MAX_PACE_SPAN_MS = 4000;
// Up to this many characters of 12 bits, the wire time product fits in 32 bits.
static constexpr size_t MAX_WIRE_TIME_CHARS_32 = UINT32_MAX / (12 * 1000000);

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

size_t UARTComponent::paced_write_room(uint32_t last_write_ms) {
  size_t room = this->available_for_write();
  if (room != SIZE_MAX) {
    return room;
  }
  // A pass woken early writes little.
  uint32_t span =
      std::min({App.get_loop_component_start_time() - last_write_ms, App.get_loop_interval(), MAX_PACE_SPAN_MS});
  // 10 bits per byte on the line.
  uint32_t paced = this->baud_rate_ / 10 * span / 1000;
  return std::max<size_t>(1, paced);
}

uint32_t UARTComponent::wire_time_us(size_t chars) const {
  const uint32_t baud_rate = std::max<uint32_t>(this->baud_rate_, 1);
  if (chars <= MAX_WIRE_TIME_CHARS_32)
    return static_cast<uint32_t>(chars) * this->bits_per_char() * 1000000 / baud_rate;
  return static_cast<uint32_t>(
      std::min<uint64_t>(static_cast<uint64_t>(chars) * this->bits_per_char() * 1000000 / baud_rate, UINT32_MAX));
}

void UARTComponent::set_rx_full_threshold_ms(uint8_t time) {
  uint8_t bytelength = this->bits_per_char();
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
