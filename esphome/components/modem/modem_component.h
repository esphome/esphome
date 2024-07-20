#pragma once

#include <memory>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/automation.h"

#ifdef USE_ESP_IDF

// esp_modem will use esphome logger (needed if other components include esphome/core/log.h)
// We need to do this because "cxx_include/esp_modem_api.hpp" is not a pure C++ header, and use logging.
// FIXME: Find another workaround ?.
// error: using declarations in the global namespace in headers are prohibited
// [google-global-names-in-headers,-warnings-as-errors]
using esphome::esp_log_printf_;  // NOLINT(google-global-names-in-headers):

#include <cxx_include/esp_modem_api.hpp>
#include <driver/gpio.h>
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
  CONNECTING,
  CONNECTED,
  DISCONNECTING,
  DISABLED,
};

enum class ModemModel { BG96, SIM800, SIM7000, SIM7070, SIM7600, UNKNOWN };

class ModemComponent : public Component {
 public:
  ModemComponent();
  void dump_config() override;
  void setup() override;
  void loop() override;
  bool is_connected();
  float get_setup_priority() const override;
  bool can_proceed() override;
  network::IPAddresses get_ip_addresses();
  std::string get_use_address() const;
  void set_use_address(const std::string &use_address);
  void set_rx_pin(gpio_num_t rx_pin) { this->rx_pin_ = rx_pin; }
  void set_tx_pin(gpio_num_t tx_pin) { this->tx_pin_ = tx_pin; }
  void set_username(const std::string &username) { this->username_ = username; }
  void set_password(const std::string &password) { this->password_ = password; }
  void set_pin_code(const std::string &pin_code) { this->pin_code_ = pin_code; }
  void set_apn(const std::string &apn) { this->apn_ = apn; }
  void set_model(const std::string &model) {
    this->model_ = this->modem_model_map_.count(model) ? modem_model_map_[model] : ModemModel::UNKNOWN;
  }
  void set_not_responding_cb(Trigger<> *not_responding_cb) { this->not_responding_cb_ = not_responding_cb; }
  void add_init_at_command(const std::string &cmd) { this->init_at_commands_.push_back(cmd); }
  std::string send_at(const std::string &cmd);
  bool get_imei(std::string &result);
  bool modem_ready();
  void enable();
  void disable();
  void add_on_state_callback(std::function<void(ModemComponentState)> &&callback);
  std::unique_ptr<DCE> dce{nullptr};

 protected:
  void reset_();
  gpio_num_t rx_pin_ = gpio_num_t::GPIO_NUM_NC;
  gpio_num_t tx_pin_ = gpio_num_t::GPIO_NUM_NC;
  std::string pin_code_;
  std::string username_;
  std::string password_;
  std::string apn_;
  std::vector<std::string> init_at_commands_;
  ModemModel model_;
  std::unordered_map<std::string, ModemModel> modem_model_map_ = {{"BG96", ModemModel::BG96},
                                                                  {"SIM800", ModemModel::SIM800},
                                                                  {"SIM7000", ModemModel::SIM7000},
                                                                  {"SIM7070", ModemModel::SIM7070},
                                                                  {"SIM7600", ModemModel::SIM7600}};
  std::shared_ptr<DTE> dte_{nullptr};
  esp_netif_t *ppp_netif_{nullptr};
  esp_modem_dte_config_t dte_config_;
  ModemComponentState state_{ModemComponentState::DISABLED};
  void start_connect_();
  bool started_{false};
  bool enabled_{false};
  bool connected_{false};
  bool got_ipv4_address_{false};
  uint32_t connect_begin_;
  static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
  void dump_connect_params_();
  std::string use_address_;
  uint32_t command_delay_ = 500;
  Trigger<> *not_responding_cb_{nullptr};
  CallbackManager<void(ModemComponentState)> on_state_callback_;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ModemComponent *global_modem_component;

}  // namespace modem
}  // namespace esphome

#endif
