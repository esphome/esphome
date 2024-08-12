#pragma once
#ifdef USE_ESP_IDF

#include <memory>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/gpio.h"
#include "esphome/core/automation.h"
#include "esphome/components/network/util.h"
#include "esphome/components/watchdog/watchdog.h"

// esp_modem will use esphome logger (needed if other components include esphome/core/log.h)
// We need to do this because "cxx_include/esp_modem_api.hpp" is not a pure C++ header, and use logging.
// error: using declarations in the global namespace in headers are prohibited
// [google-global-names-in-headers,-warnings-as-errors]
using esphome::esp_log_printf_;  // NOLINT(google-global-names-in-headers):

#include <driver/gpio.h>

#include <cxx_include/esp_modem_api.hpp>
#include <esp_modem_config.h>

#include <unordered_map>
#include <utility>

namespace esphome {
namespace modem {

using namespace esp_modem;

enum class ModemComponentState {
  NOT_RESPONDING,
  DISCONNECTED,
  CONNECTED,
  DISABLED,
};

enum class ModemPowerState {
  TON,
  TONUART,
  TOFF,
  TOFFUART,
};

struct AtCommandResult {
  std::string result;
  bool success;

  // Conversion to bool, allowing you to do things like `if (commandResult) {...}`
  operator bool() const { return success; }
};

class ModemComponent : public Component {
 public:
  void set_use_address(const std::string &use_address) { this->use_address_ = use_address; }
  void set_rx_pin(InternalGPIOPin *rx_pin) { this->rx_pin_ = rx_pin; }
  void set_tx_pin(InternalGPIOPin *tx_pin) { this->tx_pin_ = tx_pin; }
  void set_model(const std::string &model) { this->model_ = model; }
  void set_power_pin(GPIOPin *power_pin) { this->power_pin_ = power_pin; }
  void set_power_ton(int ton) { this->power_ton_ = ton; }
  void set_power_tonuart(int tonuart) { this->power_tonuart_ = tonuart; }
  void set_power_toff(int toff) { this->power_toff_ = toff; }
  void set_power_toffuart(int toffuart) { this->power_toffuart_ = toffuart; }
  void set_status_pin(GPIOPin *status_pin) { this->status_pin_ = status_pin; }
  void set_pin_code(const std::string &pin_code) { this->pin_code_ = pin_code; }
  void set_apn(const std::string &apn) { this->apn_ = apn; }
  void set_not_responding_cb(Trigger<> *not_responding_cb) { this->not_responding_cb_ = not_responding_cb; }
  void enable_cmux() { this->cmux_ = true; }
  void enable_debug();
  void add_init_at_command(const std::string &cmd) { this->init_at_commands_.push_back(cmd); }
  bool is_connected() { return this->component_state_ == ModemComponentState::CONNECTED; }
  bool is_disabled() { return this->component_state_ == ModemComponentState::DISABLED; }
  AtCommandResult send_at(const std::string &cmd, uint32_t timeout);
  AtCommandResult send_at(const std::string &cmd);
  AtCommandResult get_imei();
  bool get_power_status();
  bool modem_ready();
  bool modem_ready(bool force_check);
  void enable();
  void disable();

  network::IPAddresses get_ip_addresses();
  std::string get_use_address() const;

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)

  ModemComponent();
  void setup() override;
  void loop() override;
  void dump_config() override { this->dump_connect_params_(); }
  float get_setup_priority() const override { return setup_priority::WIFI + 1; }  // just before WIFI
  bool can_proceed() override {
    if (!this->internal_state_.enabled) {
      return true;
    }
    return this->is_connected();
  };
  void add_on_state_callback(std::function<void(ModemComponentState, ModemComponentState)> &&callback) {
    this->on_state_callback_.add(std::move(callback));
  }
  // main esp_modem object
  // https://docs.espressif.com/projects/esp-protocols/esp_modem/docs/latest/internal_docs.html#dce-internal-implementation
  std::unique_ptr<DCE> dce{nullptr};

 protected:
  void modem_lazy_init_();
  bool modem_sync_();
  bool prepare_sim_();
  void send_init_at_();
  bool is_network_attached_();
  bool start_ppp_();
  bool stop_ppp_();
  void poweron_();
  void poweroff_();
  static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
  void dump_connect_params_();

  // Attributes from yaml config
  InternalGPIOPin *tx_pin_;
  InternalGPIOPin *rx_pin_;
  std::string model_;
  GPIOPin *status_pin_{nullptr};
  int power_ton_;
  int power_tonuart_;
  int power_toff_;
  int power_toffuart_;
  GPIOPin *power_pin_{nullptr};
  std::string pin_code_;
  std::string apn_;
  std::vector<std::string> init_at_commands_;
  std::string use_address_;
  bool cmux_{false};
  // separate handler for `on_not_responding` (we want to know when it's ended)
  Trigger<> *not_responding_cb_{nullptr};
  CallbackManager<void(ModemComponentState, ModemComponentState)> on_state_callback_;

  // Allow changes from yaml ?
  size_t uart_rx_buffer_size_ = 2048;         // 256-2048
  size_t uart_tx_buffer_size_ = 1024;         // 256-2048
  uint8_t uart_event_queue_size_ = 30;        // 10-40
  size_t uart_event_task_stack_size_ = 2048;  // 2000-6000
  uint8_t uart_event_task_priority_ = 5;      // 3-22
  uint32_t command_delay_ = 10000;            // timeout for AT commands
  uint32_t update_interval_ = 60 * 1000;

  // Changes will trigger user callback
  ModemComponentState component_state_{ModemComponentState::DISABLED};

  // the uart DTE
  // https://docs.espressif.com/projects/esp-protocols/esp_modem/docs/latest/internal_docs.html#_CPPv4N9esp_modem3DCEE
  std::shared_ptr<DTE> dte_{nullptr};
  esp_netif_t *ppp_netif_{nullptr};

  struct InternalState {
    bool start{false};
    bool enabled{false};
    bool connected{false};
    bool got_ipv4_address{false};
    // true if modem_sync_ was sucessfull
    bool modem_synced{false};
    // date start (millis())
    uint32_t connect_begin;
    // guess power state
    bool powered_on{false};
    // Will be true when power transitionning
    bool power_transition{false};
    // states for triggering on/off signals
    ModemPowerState power_state{ModemPowerState::TOFFUART};
  };
  InternalState internal_state_;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ModemComponent *global_modem_component;

}  // namespace modem
}  // namespace esphome

#endif
