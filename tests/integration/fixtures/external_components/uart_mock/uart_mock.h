#pragma once

// ============================================================================
// HOST-ONLY TEST COMPONENT — DO NOT COPY TO PRODUCTION CODE
//
// This component runs exclusively on the host platform for integration testing.
// It intentionally uses std::vector, std::deque, and dynamic allocation which
// would be inappropriate for production embedded components. Do not use this
// code as a reference for writing ESPHome components targeting real hardware.
// ============================================================================

#include "esphome/core/component.h"
#include "esphome/components/uart/uart_component.h"
#include <deque>
#include <vector>

namespace esphome::uart_mock {

class MockUartComponent : public uart::UARTComponent, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  // UARTComponent interface
  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override;
  void flush() override;
  void set_rx_full_threshold(size_t rx_full_threshold) override;
  void set_rx_timeout(size_t rx_timeout) override;

  // Scenario configuration - called from generated code
  void add_injection(const std::vector<uint8_t> &rx_data, uint32_t delay_ms);
  void add_response(const std::vector<uint8_t> &expect_tx, const std::vector<uint8_t> &inject_rx);
  void add_periodic_rx(const std::vector<uint8_t> &data, uint32_t interval_ms);

  void set_tx_hook(std::function<void(const std::vector<uint8_t> &)> &&cb) { this->tx_hook_ = std::move(cb); }
  void inject_to_rx_buffer(const std::vector<uint8_t> &data);
  void inject_to_rx_buffer(const uint8_t *data, size_t len);

 protected:
  void check_logger_conflict() override {}
  void try_match_response_();

  // Timed injections
  struct Injection {
    std::vector<uint8_t> rx_data;
    uint32_t delay_ms;
  };
  std::vector<Injection> injections_;
  uint32_t injection_index_{0};
  uint32_t scenario_start_ms_{0};
  uint32_t cumulative_delay_ms_{0};
  bool loop_started_{false};

  // TX-triggered responses
  struct Response {
    std::vector<uint8_t> expect_tx;
    std::vector<uint8_t> inject_rx;
  };
  std::vector<Response> responses_;
  std::vector<uint8_t> tx_buffer_;

  // RX buffer
  std::deque<uint8_t> rx_buffer_;

  // Periodic RX
  struct PeriodicRx {
    std::vector<uint8_t> data;
    uint32_t interval_ms;
    uint32_t last_inject_ms{0};
  };
  std::vector<PeriodicRx> periodic_rx_;

  // Observability
  uint32_t tx_count_{0};
  uint32_t rx_count_{0};

  // Direct TX hook for tests that want to bypass the response-matching logic
  std::function<void(const std::vector<uint8_t> &)> tx_hook_;
};

}  // namespace esphome::uart_mock
