#pragma once
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "esphome/components/uart/uart_component.h"
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "esphome/components/cdc_acm_uart/bridge/cdc_acm_uart_bridge.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::uart_mux {

/// Shares one hardware UART between a CDC-ACM UART bridge and local consumers. Local
/// consumers bind to the mux as their UART; it forwards to the hardware UART only
/// while routed locally and reports the route through is_connected(). Routing is
/// driven by the select_*() actions, typically from tinyusb's on_mount/on_unmount.
class UARTMux final : public uart::UARTComponent, public Component {
 public:
  explicit UARTMux(cdc_acm_uart::CDCACMUARTBridge *bridge) : uart_(bridge->get_uart_parent()), bridge_(bridge) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  /// Route locally at boot instead of leaving the UART with the bridge.
  void set_start_local(bool start_local) { this->start_local_ = start_local; }

  /// Pause the bridge and route the UART to local consumers once it has stopped.
  void select_local();
  /// Route the UART back to the bridge.
  void select_bridge();
  bool is_local() const { return this->route_ == Route::ROUTE_LOCAL; }

  // uart::UARTComponent: forwarded while routed locally, inert otherwise.
  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override { return this->is_local() && this->uart_->peek_byte(data); }
  bool read_array(uint8_t *data, size_t len) override { return this->is_local() && this->uart_->read_array(data, len); }
  size_t available() override { return this->is_local() ? this->uart_->available() : 0; }
  uart::UARTFlushResult flush() override {
    return this->is_local() ? this->uart_->flush() : uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS;
  }
  bool is_connected() override { return this->is_local(); }
  // Ignored: the bridge's tasks block inside the driver, and reinstalling it would
  // pull it out from under them. The framing is the hardware UART's to change.
  void load_settings(bool dump_config) override;
  using UARTComponent::load_settings;

 protected:
  enum class Route : uint8_t {
    ROUTE_BRIDGE,
    ROUTE_PENDING_LOCAL,  // pause() requested; the bridge may still be on the bus
    ROUTE_LOCAL,
  };

  void check_logger_conflict() override {}
  void flush_input_();

  uart::IDFUARTComponent *uart_;
  cdc_acm_uart::CDCACMUARTBridge *bridge_;
  Route route_{Route::ROUTE_BRIDGE};
  bool start_local_{false};
};

template<typename... Ts> class SelectLocalAction final : public Action<Ts...> {
 public:
  explicit SelectLocalAction(UARTMux *parent) : parent_(parent) {}
  void play(const Ts &...) override { this->parent_->select_local(); }

 protected:
  UARTMux *parent_;
};

template<typename... Ts> class SelectBridgeAction final : public Action<Ts...> {
 public:
  explicit SelectBridgeAction(UARTMux *parent) : parent_(parent) {}
  void play(const Ts &...) override { this->parent_->select_bridge(); }

 protected:
  UARTMux *parent_;
};

template<typename... Ts> class IsLocalCondition final : public Condition<Ts...> {
 public:
  explicit IsLocalCondition(UARTMux *parent) : parent_(parent) {}
  bool check(const Ts &...) override { return this->parent_->is_local(); }

 protected:
  UARTMux *parent_;
};

}  // namespace esphome::uart_mux
#endif
