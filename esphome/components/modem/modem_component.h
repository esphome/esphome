#pragma once
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/preferences.h"
#include "esphome/components/network/ip_address.h"
#ifdef USE_MODEM_URC
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <cxx_include/esp_modem_api.hpp>
#include <esp_modem_config.h>

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace esphome::modem {

using namespace esp_modem;

enum class ModemComponentState {
  MODEM_DISABLED,
  MODEM_ENABLING,
  MODEM_SYNCED,
  MODEM_CONNECTING,
  MODEM_WAIT_IP,
  MODEM_CONNECTED,
  MODEM_DISCONNECTING,
};

enum class SimResult {
  SIM_RESULT_READY,
  SIM_RESULT_RETRY,  // status not readable yet, or the PIN was just sent
  SIM_RESULT_FATAL,  // locked with no usable PIN, or PUK locked
};

struct AtCommandResult {
  std::string output;
  bool success{false};
};

// Every esp_modem factory shares this signature, so codegen picks the one for the model
using DceFactory = std::unique_ptr<DCE> (*)(const dce_config *, std::shared_ptr<DTE>, esp_netif_t *);

class ModemComponent : public Component {
 public:
  ModemComponent();
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI + 1; }  // Just before Wi-Fi

  void set_reboot_timeout(uint32_t timeout) { this->timeout_ = timeout; }
  void set_use_address(const char *use_address) { this->use_address_ = use_address; }
  void set_rx_pin(InternalGPIOPin *rx_pin) { this->rx_pin_ = rx_pin; }
  void set_tx_pin(InternalGPIOPin *tx_pin) { this->tx_pin_ = tx_pin; }
  void set_rts_pin(InternalGPIOPin *rts_pin) { this->rts_pin_ = rts_pin; }
  void set_cts_pin(InternalGPIOPin *cts_pin) { this->cts_pin_ = cts_pin; }
  void set_baud_rate(int baud_rate) { this->baud_rate_ = baud_rate; }
  void set_model(const char *model, DceFactory factory) {
    this->model_ = model;
    this->dce_factory_ = factory;
  }
  void set_pin_code(const char *pin_code) { this->pin_code_ = pin_code; }
  void set_tx_buffer_size(uint16_t tx_buffer_size) { this->tx_buffer_size_ = tx_buffer_size; }
  void set_rx_buffer_size(uint16_t rx_buffer_size) { this->rx_buffer_size_ = rx_buffer_size; }
  void set_dte_buffer_size(uint16_t dte_buffer_size) { this->dte_buffer_size_ = dte_buffer_size; }
  void set_apn(const char *apn) { this->apn_ = apn; }
  void set_init_at_commands(std::initializer_list<const char *> cmds) { this->init_at_commands_ = cmds; }
#ifdef USE_MODEM_URC
  void set_urc_text_sensor(text_sensor::TextSensor *urc_text_sensor) { this->urc_text_sensor_ = urc_text_sensor; }
#endif
  template<typename F> void add_on_state_callback(F &&callback) {
    this->on_state_callback_.add(std::forward<F>(callback));
  }

  bool is_connected() { return this->component_state_ == ModemComponentState::MODEM_CONNECTED; }
  bool is_disabled() {
    return this->component_state_ == ModemComponentState::MODEM_DISABLED &&
           this->target_state_ == ModemComponentState::MODEM_DISABLED;
  }
  bool is_enabled() { return !this->is_disabled(); }
  void enable();
  void disable();
  /// Send an AT command; with verbose the command, result and response are logged.
  AtCommandResult send_at(const std::string &cmd, uint32_t timeout = 0, bool verbose = false);

  network::IPAddresses get_ip_addresses();
  const char *get_use_address() const { return this->use_address_; };

 protected:
  ModemComponentState compute_next_state_();
  ModemComponentState handle_state_enabling_();
  ModemComponentState handle_state_synced_();
  ModemComponentState handle_state_connecting_();
  ModemComponentState handle_state_wait_ip_();
  ModemComponentState handle_state_connected_();
  ModemComponentState handle_state_disconnecting_();
  void transition_to_(ModemComponentState next_state);
  void on_enter_state_(ModemComponentState state);
  void abort_(const char *message);
  void loop_delay_(uint32_t delay_ms);
  void dump_connect_params_();

  void create_dte_dce_(int baud_rate);
  bool try_baud_(int baud);
  SimResult prepare_sim_();
  void log_status_();
  static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

  static constexpr uint32_t CONNECT_TIMEOUT_ID = 0;

  InternalGPIOPin *tx_pin_{nullptr};
  InternalGPIOPin *rx_pin_{nullptr};
  InternalGPIOPin *rts_pin_{nullptr};
  InternalGPIOPin *cts_pin_{nullptr};
  const char *model_{""};
  DceFactory dce_factory_{nullptr};
  const char *apn_{""};
  const char *pin_code_{nullptr};
  std::vector<const char *> init_at_commands_;
  uint16_t tx_buffer_size_{512};
  uint16_t rx_buffer_size_{512};
  uint16_t dte_buffer_size_{512};
  int baud_rate_{0};  // 0 = use the esp_modem default
  uint32_t timeout_{0};
#ifdef USE_MODEM_URC
  text_sensor::TextSensor *urc_text_sensor_{nullptr};
#endif

  std::unique_ptr<DCE> dce_;
  std::shared_ptr<DTE> dte_;
  esp_netif_t *ppp_netif_{nullptr};
  uart_port_t uart_port_{UART_NUM_MAX};
  int current_baud_rate_{0};
  int restored_baud_{0};
  ESPPreferenceObject pref_;
  uint8_t baud_index_{0};
  // One PIN attempt per enable(): a second wrong PIN could PUK-lock the SIM
  bool pin_sent_{false};
  // Written last by the IP event task, read by loop()
  std::atomic<bool> got_ip_{false};

  LazyCallbackManager<void(ModemComponentState)> on_state_callback_;
  ModemComponentState component_state_{ModemComponentState::MODEM_DISABLED};
  ModemComponentState target_state_{ModemComponentState::MODEM_DISABLED};
  uint32_t next_loop_millis_{0};
  uint32_t wait_ip_start_{0};

 private:
  // Points at a string literal from code generation, never at dynamic storage
  const char *use_address_{""};
};

extern ModemComponent *global_modem_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::modem

#endif
