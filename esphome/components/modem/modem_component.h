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
// FIXME: Find another workaround ?.
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

static const char *const TAG = "modem";

enum class ModemComponentState {
  NOT_RESPONDING,
  DISCONNECTED,
  CONNECTED,
  DISABLED,
};

#ifdef USE_MODEM_POWER
enum class ModemPowerState {
  TON,
  TONUART,
  TOFF,
  TOFFUART,
};
#endif  // USE_MODEM_POWER

class ModemComponent : public Component {
 public:
  ModemComponent();
  void setup() override;
  void loop() override;
  void dump_config() override;
  bool is_connected();
  float get_setup_priority() const override;
  bool can_proceed() override;
  network::IPAddresses get_ip_addresses();
  std::string get_use_address() const;
  void set_use_address(const std::string &use_address);
  void set_rx_pin(InternalGPIOPin *rx_pin) { this->rx_pin_ = rx_pin; }
  void set_tx_pin(InternalGPIOPin *tx_pin) { this->tx_pin_ = tx_pin; }
  void set_power_pin(GPIOPin *power_pin) { this->power_pin_ = power_pin; }
  void set_status_pin(GPIOPin *status_pin) { this->status_pin_ = status_pin; }
  void set_username(const std::string &username) { this->username_ = username; }
  void set_password(const std::string &password) { this->password_ = password; }
  void set_pin_code(const std::string &pin_code) { this->pin_code_ = pin_code; }
  void set_apn(const std::string &apn) { this->apn_ = apn; }
  void set_not_responding_cb(Trigger<> *not_responding_cb) { this->not_responding_cb_ = not_responding_cb; }
  void enable_cmux() { this->cmux_ = true; }
  void add_init_at_command(const std::string &cmd) { this->init_at_commands_.push_back(cmd); }
  std::string send_at(const std::string &cmd);
  bool get_imei(std::string &result);
  bool get_power_status();
  bool modem_ready();
  void enable();
  void disable();
  void add_on_state_callback(std::function<void(ModemComponentState, ModemComponentState)> &&callback);
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
  void dump_dce_status_();
  InternalGPIOPin *tx_pin_;
  InternalGPIOPin *rx_pin_;
  GPIOPin *status_pin_{nullptr};
  GPIOPin *power_pin_{nullptr};
  std::string pin_code_;
  std::string username_;
  std::string password_;
  std::string apn_;
  std::vector<std::string> init_at_commands_;
  size_t uart_rx_buffer_size_ = 2048;         // 256-2048
  size_t uart_tx_buffer_size_ = 1024;         // 256-2048
  uint8_t uart_event_queue_size_ = 30;        // 10-40
  size_t uart_event_task_stack_size_ = 2048;  // 2000-6000
  uint8_t uart_event_task_priority_ = 5;      // 3-22
  std::shared_ptr<DTE> dte_{nullptr};
  esp_netif_t *ppp_netif_{nullptr};
  ModemComponentState state_{ModemComponentState::DISABLED};
  std::shared_ptr<watchdog::WatchdogManager> watchdog_;
  bool cmux_{false};
  bool start_{false};
  bool enabled_{false};
  bool connected_{false};
  bool got_ipv4_address_{false};
  // true if modem_sync_ was sucessfull
  bool modem_synced_{false};
  // date start (millis())
  uint32_t connect_begin_;
  std::string use_address_;
  // timeout for AT commands
  uint32_t command_delay_ = 500;
  // guess power state
  bool powered_on_{false};
#ifdef USE_MODEM_POWER
  // Will be true when power transitionning
  bool power_transition_{false};
  // states for triggering on/off signals
  ModemPowerState power_state_{ModemPowerState::TOFFUART};
#endif  // USE_MODEM_POWER
  // separate handler for `on_not_responding` (we want to know when it's ended)
  Trigger<> *not_responding_cb_{nullptr};
  CallbackManager<void(ModemComponentState, ModemComponentState)> on_state_callback_;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ModemComponent *global_modem_component;

}  // namespace modem
}  // namespace esphome

#endif
