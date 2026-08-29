#include "uart_emulator.h"

#ifdef USE_ZEPHYR

#include <utility>

#include "esphome/core/log.h"

namespace esphome::zephyr {

static const char *const TAG = "zephyr.uart_emulator";

void ZephyrUartEmulator::add_trigger(const uint8_t *pattern, uint8_t pattern_len, size_t response_count) {
  if (this->triggers_.size() >= this->triggers_.capacity()) {
    ESP_LOGE(TAG, "Cannot register trigger: emulator trigger table full");
    return;
  }
  TriggerEntry entry;
  entry.pattern = pattern;
  entry.pattern_len = pattern_len;
  entry.responses.init(response_count);

  // Precompute the KMP failure function: failure[i] is the length of the longest
  // proper prefix of pattern[0..i] that is also a suffix of it. Computed and appended
  // in increasing index order so push_back() (not direct indexing) can be used.
  entry.failure.init(pattern_len);
  uint8_t k = 0;
  if (pattern_len > 0) {
    entry.failure.push_back(0);
  }
  for (uint8_t i = 1; i < pattern_len; i++) {
    while (k > 0 && pattern[i] != pattern[k]) {
      k = entry.failure[k - 1];
    }
    if (pattern[i] == pattern[k]) {
      k++;
    }
    entry.failure.push_back(k);
  }

  this->triggers_.push_back(std::move(entry));
}

void ZephyrUartEmulator::add_response(const uint8_t *data, uint16_t len) {
  if (this->triggers_.empty()) {
    ESP_LOGE(TAG, "Cannot add response: no trigger registered yet");
    return;
  }
  TriggerEntry &trigger = this->triggers_[this->triggers_.size() - 1];
  if (trigger.responses.size() >= trigger.responses.capacity()) {
    ESP_LOGE(TAG, "Cannot add response: trigger response table full");
    return;
  }
  trigger.responses.push_back(ResponseEntry{data, len});
}

void ZephyrUartEmulator::push_rx(const uint8_t *data, size_t len) { uart_emul_put_rx_data(this->uart_dev_, data, len); }

void ZephyrUartEmulator::feed_byte_(uint8_t byte) {
  for (auto &trigger : this->triggers_) {
    // Standard KMP step: back off to shorter matched prefixes until the current byte
    // extends one of them (or none do, leaving match_pos at 0).
    while (trigger.match_pos > 0 && trigger.pattern[trigger.match_pos] != byte) {
      trigger.match_pos = trigger.failure[trigger.match_pos - 1];
    }
    if (trigger.pattern[trigger.match_pos] == byte) {
      trigger.match_pos++;
    }
    if (trigger.match_pos == trigger.pattern_len) {
      trigger.match_pos = 0;
      if (!trigger.responses.empty()) {
        const ResponseEntry &response = trigger.responses[trigger.cycle_index];
        uart_emul_put_rx_data(this->uart_dev_, response.data, response.len);
        trigger.cycle_index = (trigger.cycle_index + 1) % trigger.responses.size();
      }
    }
  }
}

void ZephyrUartEmulator::tx_data_ready_(size_t size) {
  uint8_t buf[32];
  while (size > 0) {
    uint32_t chunk = uart_emul_get_tx_data(this->uart_dev_, buf, sizeof(buf));
    if (chunk == 0)
      break;
    for (uint32_t i = 0; i < chunk; i++)
      this->feed_byte_(buf[i]);
    size -= chunk;
  }
}

void ZephyrUartEmulator::tx_data_ready_s(const device *dev, size_t size, void *user_data) {
  static_cast<ZephyrUartEmulator *>(user_data)->tx_data_ready_(size);
}

void ZephyrUartEmulator::setup() { uart_emul_callback_tx_data_ready_set(this->uart_dev_, tx_data_ready_s, this); }

void ZephyrUartEmulator::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Emulator: %u trigger(s)", static_cast<unsigned>(this->triggers_.size()));
}

}  // namespace esphome::zephyr

#endif
