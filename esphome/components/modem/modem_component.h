#pragma once

#include <memory>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

#ifdef USE_ESP_IDF

using esphome::esp_log_printf_;  // esp_modem will use esphome logger (needed if other components include
                                 // esphome/core/log.h)
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
  STOPPED,
  CONNECTING,
  CONNECTED,
};

enum class ModemModel { BG96, SIM800, SIM7000, SIM7070, SIM7070_GNSS, SIM7600, UNKNOWN };

class ModemComponent : public Component {
 public:
  ModemComponent();
  void dump_config() override;
  void setup() override;
  void loop() override;
  bool is_connected();
  float get_setup_priority() const override;
  bool can_proceed() override;
  void on_shutdown() override { powerdown(); }
  network::IPAddresses get_ip_addresses();
  std::string get_use_address() const;
  void set_use_address(const std::string &use_address);
  void poweron();
  void powerdown();
  void set_rx_pin(gpio_num_t rx_pin) { this->rx_pin_ = rx_pin; }
  void set_tx_pin(gpio_num_t tx_pin) { this->tx_pin_ = tx_pin; }
  void set_power_pin(gpio_num_t power_pin) { this->power_pin_ = power_pin; }
  void set_flight_pin(gpio_num_t flight_pin) { this->flight_pin_ = flight_pin; }
  void set_username(const std::string username) { this->username_ = std::move(username); }
  void set_password(const std::string password) { this->password_ = std::move(password); }
  void set_pin_code(const std::string pin_code) { this->pin_code_ = std::move(pin_code); }
  void set_apn(const std::string apn) { this->apn_ = std::move(apn); }
  void set_status_pin(gpio_num_t status_pin) { this->status_pin_ = status_pin; }
  void set_dtr_pin(gpio_num_t dtr_pin) { this->dtr_pin_ = dtr_pin; }
  void set_model(const std::string &model) {
    this->model_ = this->modem_model_map_.count(model) ? modem_model_map_[model] : ModemModel::UNKNOWN;
  }
  bool get_status() { return gpio_get_level(this->status_pin_); }
  std::unique_ptr<DCE> dce;

 protected:
  gpio_num_t rx_pin_ = gpio_num_t::GPIO_NUM_NC;
  gpio_num_t tx_pin_ = gpio_num_t::GPIO_NUM_NC;
  gpio_num_t power_pin_ = gpio_num_t::GPIO_NUM_NC;
  gpio_num_t flight_pin_ = gpio_num_t::GPIO_NUM_NC;
  gpio_num_t status_pin_ = gpio_num_t::GPIO_NUM_NC;
  gpio_num_t dtr_pin_ = gpio_num_t::GPIO_NUM_NC;

  std::string pin_code_;
  std::string username_;
  std::string password_;
  std::string apn_;
  ModemModel model_;
  std::unordered_map<std::string, ModemModel> modem_model_map_ = {{"BG96", ModemModel::BG96},
                                                                  {"SIM800", ModemModel::SIM800},
                                                                  {"SIM7000", ModemModel::SIM7000},
                                                                  {"SIM7070", ModemModel::SIM7070},
                                                                  {"SIM7070_GNSS", ModemModel::SIM7070_GNSS},
                                                                  {"SIM7600", ModemModel::SIM7600}};
  std::shared_ptr<DTE> dte_;
  esp_netif_t *ppp_netif_{nullptr};
  esp_modem_dte_config_t dte_config_;
  ModemComponentState state_{ModemComponentState::STOPPED};
  void start_connect_();
  bool started_{false};
  bool connected_{false};
  bool got_ipv4_address_{false};
  uint32_t connect_begin_;
  static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
  void dump_connect_params_();
  std::string use_address_;
  void config_gpio_();
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ModemComponent *global_modem_component;

}  // namespace modem
}  // namespace esphome

#endif
