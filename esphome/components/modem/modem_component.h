#pragma once
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/gpio.h"
#include "esphome/core/automation.h"
#include "esphome/core/preferences.h"
#include "esphome/components/network/ip_address.h"
#ifdef USE_MODEM_URC
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include <unordered_map>
#include <memory>

#include "modem_handler.h"
namespace esphome {
namespace modem {

enum class ModemComponentState {
  MODEM_DISABLED,
  MODEM_ENABLING,
  MODEM_SYNCED,
  MODEM_CONNECTING,  // Merges INIT_NETWORK + START_PPP
  MODEM_WAIT_IP,
  MODEM_CONNECTED,
  MODEM_DISCONNECTING,  // Renamed from DISABLING
};

struct ModemRestoreState {
  int baud_rate{0};
} __attribute__((packed));

class ModemComponent : public Component {
 public:
  void set_reboot_timeout(uint32_t timeout) { this->timeout_ = timeout; }
  void set_use_address(const char *use_address) { this->use_address_ = use_address; }
  // Setters now modify the handler's attributes
  void set_rx_pin(InternalGPIOPin *rx_pin) { this->modem_handler->rx_pin = rx_pin; }
  void set_tx_pin(InternalGPIOPin *tx_pin) { this->modem_handler->tx_pin = tx_pin; }
  void set_rts_pin(InternalGPIOPin *rts_pin) { this->modem_handler->rts_pin = rts_pin; }
  void set_cts_pin(InternalGPIOPin *cts_pin) { this->modem_handler->cts_pin = cts_pin; }
  void set_baud_rate(int baud_rate) { this->modem_handler->baud_rate = baud_rate; }
  void set_model(const std::string &model) { this->modem_handler->model = model; }
  void set_pin_code(const std::string &pin_code) { this->modem_handler->pin_code = pin_code; }
  void set_tx_buffer_size(uint16_t tx_buffer_size) { this->modem_handler->tx_buffer_size = tx_buffer_size; }
  void set_rx_buffer_size(uint16_t rx_buffer_size) { this->modem_handler->rx_buffer_size = rx_buffer_size; }
  void set_dte_buffer_size(uint16_t dte_buffer_size) { this->modem_handler->dte_buffer_size = dte_buffer_size; }
  void set_apn(const std::string &apn) { this->modem_handler->apn = apn; }
  void add_init_at_command(const std::string &cmd) { this->modem_handler->init_at_commands.push_back(cmd); }
#ifdef USE_MODEM_URC
  void set_urc_text_sensor(text_sensor::TextSensor *urc_text_sensor) { this->urc_text_sensor = urc_text_sensor; }
  text_sensor::TextSensor *urc_text_sensor{nullptr};
#endif
  bool is_connected() { return this->component_state_ == ModemComponentState::MODEM_CONNECTED; }
  bool is_disabled() {
    return this->component_state_ == ModemComponentState::MODEM_DISABLED &&
           this->target_state_ == ModemComponentState::MODEM_DISABLED;
  }
  bool is_enabled() { return !is_disabled(); }

  // Delegated methods
  AtCommandResult send_at(const std::string &cmd, uint32_t timeout = 0, bool verbose = false);
  void enable();
  void disable();

  network::IPAddresses get_ip_addresses();
  const char *get_use_address() const { return this->use_address_; };

  // ========== INTERNAL METHODS ==========
  // (In most use cases, you won't need these)

  ModemComponent();
  void setup() override;
  void loop() override;

  void dump_config() override { this->dump_connect_params_(); }
  float get_setup_priority() const override { return setup_priority::WIFI + 1; }  // Just before Wi-Fi
  void add_on_state_callback(std::function<void(ModemComponentState)> &&callback) {
    this->on_state_callback_.add(std::move(callback));
  }
  std::unique_ptr<ModemHandler> modem_handler{nullptr};

 protected:
  // ===== State handler methods =====
  ModemComponentState handle_state_disabled_();
  ModemComponentState handle_state_enabling_();
  ModemComponentState handle_state_synced_();
  ModemComponentState handle_state_connecting_();  // Merges INIT_NETWORK + START_PPP
  ModemComponentState handle_state_wait_ip_();
  ModemComponentState handle_state_connected_();
  ModemComponentState handle_state_disconnecting_();

  void transition_to_(ModemComponentState next_state);
  void on_enter_state_(ModemComponentState state);

  // target_state_ model helpers
  ModemComponentState compute_next_state_();
  bool is_going_up_() const { return target_state_ == ModemComponentState::MODEM_CONNECTED; }
  bool is_going_down_() const { return target_state_ == ModemComponentState::MODEM_DISABLED; }

  void abort_(const std::string &message);
  void loop_delay_(uint32_t delay_ms);
  void dump_connect_params_();

  // Attributes from YAML config
  uint32_t timeout_;
  CallbackManager<void(ModemComponentState)> on_state_callback_;

  // Changes will trigger user callback
  ModemComponentState component_state_{ModemComponentState::MODEM_DISABLED};
  ModemComponentState target_state_{ModemComponentState::MODEM_DISABLED};  // Stable target state

  uint32_t next_loop_millis_{0};
  uint8_t wait_ip_retry_{0};

  ModemRestoreState modem_restore_state_{};
  ESPPreferenceObject pref_;

 private:
  // Stores a pointer to a string literal (static storage duration).
  // ONLY set from Python-generated code with string literals - never dynamic strings.
  const char *use_address_{""};
};

extern ModemComponent *global_modem_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace modem
}  // namespace esphome

#endif
