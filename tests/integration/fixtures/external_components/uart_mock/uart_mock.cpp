// Host-only test component — do not copy to production code.
// See uart_mock.h for details.

#include "uart_mock.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::uart_mock {

static const char *const TAG = "uart_mock";

void MockUartComponent::setup() {
  ESP_LOGI(TAG, "Mock UART initialized with %zu injections, %zu responses, %zu periodic", this->injections_.size(),
           this->responses_.size(), this->periodic_rx_.size());
}

void MockUartComponent::loop() {
  if (!this->loop_started_) {
    this->loop_started_ = true;
    if (this->auto_start_) {
      this->start_scenario();
    } else {
      ESP_LOGD(TAG, "Scenario waiting for manual start");
    }
  }

  if (!this->scenario_active_) {
    return;
  }

  uint32_t now = App.get_loop_component_start_time();

  // Process at most ONE timed injection per loop iteration.
  // This ensures each injection is in a separate loop cycle, giving the consuming
  // component (e.g., LD2410) a chance to process each batch independently.
  if (this->injection_index_ < this->injections_.size()) {
    auto &injection = this->injections_[this->injection_index_];
    uint32_t total_delay = this->cumulative_delay_ms_ + injection.delay_ms;
    if (now - this->scenario_start_ms_ >= total_delay) {
      ESP_LOGD(TAG, "Injecting %zu RX bytes (injection %u)", injection.rx_data.size(), this->injection_index_);
      this->inject_to_rx_buffer(injection.rx_data);
      this->cumulative_delay_ms_ += injection.delay_ms;
      this->injection_index_++;
    }
  }

  // Process periodic RX
  for (auto &periodic : this->periodic_rx_) {
    if (now - periodic.last_inject_ms >= periodic.interval_ms) {
      this->inject_to_rx_buffer(periodic.data);
      periodic.last_inject_ms = now;
    }
  }

  // Process delayed responses
  for (auto &response : this->responses_) {
    if (response.delay_ms > 0 && response.last_match_ms > 0 && now - response.last_match_ms >= response.delay_ms) {
      ESP_LOGD(TAG, "Injecting %zu RX bytes for delayed response", response.inject_rx.size());
      this->inject_to_rx_buffer(response.inject_rx);
      response.last_match_ms = 0;  // Reset to prevent repeated injection
    }
  }
}

void MockUartComponent::start_scenario() {
  uint32_t now = App.get_loop_component_start_time();
  this->scenario_active_ = true;
  this->scenario_start_ms_ = now;
  this->cumulative_delay_ms_ = 0;
  this->injection_index_ = 0;
  this->tx_buffer_.clear();
  for (auto &periodic : this->periodic_rx_) {
    periodic.last_inject_ms = now;
  }
  ESP_LOGD(TAG, "Scenario started at %u ms", now);
}

void MockUartComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Mock UART Component:\n"
                "  Baud Rate: %u\n"
                "  Injections: %zu\n"
                "  Responses: %zu\n"
                "  Periodic RX: %zu",
                this->baud_rate_, this->injections_.size(), this->responses_.size(), this->periodic_rx_.size());
}

void MockUartComponent::write_array(const uint8_t *data, size_t len) {
  this->tx_count_ += len;
  this->tx_buffer_.insert(this->tx_buffer_.end(), data, data + len);

  // Log all TX data so tests can verify what the component sends
  if (len > 0 && len <= 64) {
    char hex_buf[format_hex_pretty_size(64)];
    ESP_LOGD(TAG, "TX %zu bytes: %s", len, format_hex_pretty_to(hex_buf, sizeof(hex_buf), data, len));
  } else if (len > 64) {
    ESP_LOGD(TAG, "TX %zu bytes (too large to log)", len);
  }

#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(uart::UART_DIRECTION_TX, data[i]);
  }
#endif

  if (this->scenario_active_) {
    this->try_match_response_();
  }

  // This directly calls a tx_hook (lambda) as an alternative to the simpler match_response mechanism.
  if (this->tx_hook_ && this->scenario_active_) {
    std::vector<uint8_t> buf(data, data + len);
    this->tx_hook_(buf);
  }
}

bool MockUartComponent::peek_byte(uint8_t *data) {
  if (this->rx_buffer_.empty()) {
    return false;
  }
  *data = this->rx_buffer_.front();
  return true;
}

bool MockUartComponent::read_array(uint8_t *data, size_t len) {
  if (this->rx_buffer_.size() < len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    data[i] = this->rx_buffer_.front();
    this->rx_buffer_.pop_front();
  }
  this->rx_count_ += len;

#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(uart::UART_DIRECTION_RX, data[i]);
  }
#endif

  return true;
}

size_t MockUartComponent::available() { return this->rx_buffer_.size(); }

void MockUartComponent::flush() {
  // Nothing to flush in mock
}

void MockUartComponent::set_rx_full_threshold(size_t rx_full_threshold) {
  this->rx_full_threshold_ = rx_full_threshold;
}

void MockUartComponent::set_rx_timeout(size_t rx_timeout) { this->rx_timeout_ = rx_timeout; }

void MockUartComponent::add_injection(const std::vector<uint8_t> &rx_data, uint32_t delay_ms) {
  this->injections_.push_back({rx_data, delay_ms});
}

void MockUartComponent::add_response(const std::vector<uint8_t> &expect_tx, const std::vector<uint8_t> &inject_rx,
                                     uint32_t delay_ms) {
  this->responses_.push_back({expect_tx, inject_rx, delay_ms, 0});
}

void MockUartComponent::add_periodic_rx(const std::vector<uint8_t> &data, uint32_t interval_ms) {
  this->periodic_rx_.push_back({data, interval_ms, 0});
}

void MockUartComponent::try_match_response_() {
  for (auto &response : this->responses_) {
    if (this->tx_buffer_.size() < response.expect_tx.size()) {
      continue;
    }
    // Check if tx_buffer_ ends with expect_tx
    size_t offset = this->tx_buffer_.size() - response.expect_tx.size();
    if (std::equal(response.expect_tx.begin(), response.expect_tx.end(), this->tx_buffer_.begin() + offset)) {
      ESP_LOGD(TAG, "TX match found, injecting %zu RX bytes", response.inject_rx.size());
      if (response.delay_ms > 0) {
        ESP_LOGD(TAG, "Delaying response by %u ms", response.delay_ms);
        // Schedule the response injection as a future injection
        response.last_match_ms = App.get_loop_component_start_time();
      } else {
        this->inject_to_rx_buffer(response.inject_rx);
      }
      this->tx_buffer_.clear();
      return;
    }
  }
}

void MockUartComponent::inject_to_rx_buffer(const uint8_t *data, size_t len) {
  std::vector<uint8_t> vec(data, data + len);
  this->inject_to_rx_buffer(vec);
}

void MockUartComponent::inject_to_rx_buffer(const std::vector<uint8_t> &data) {
  // Log injected RX data so tests can see what's being fed to the component
  if (!data.empty() && data.size() <= 64) {
    char hex_buf[format_hex_pretty_size(64)];
    ESP_LOGD(TAG, "RX inject %zu bytes: %s", data.size(),
             format_hex_pretty_to(hex_buf, sizeof(hex_buf), data.data(), data.size()));
  } else if (data.size() > 64) {
    ESP_LOGD(TAG, "RX inject %zu bytes (too large to log inline)", data.size());
  }
  for (uint8_t byte : data) {
    this->rx_buffer_.push_back(byte);
  }
}

}  // namespace esphome::uart_mock
