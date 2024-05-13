#pragma once

#include <memory>
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

#ifdef USE_ESP_IDF

using esphome::esp_log_printf_;  // esp_modem will use esphome logger (needed if other components include
                                 // esphome/core/log.h)
#include <cxx_include/esp_modem_api.hpp>
#include <esp_modem_config.h>
#include <driver/gpio.h>
#include <unordered_map>

namespace esphome {
namespace gsm {

static const char *const TAG = "gsm";

enum class GSMComponentState {
  STOPPED,
  CONNECTING,
  CONNECTED,
};

enum class GSMModel { BG96, SIM800, SIM7000, SIM7070, SIM7070_GNSS, SIM7600, UNKNOWN };

class GSMComponent : public Component {
 public:
  GSMComponent();
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
  void set_username(std::string username) { this->username_ = username; }
  void set_password(std::string password) { this->password_ = password; }
  void set_pin_code(std::string pin_code) { this->pin_code_ = pin_code; }
  void set_apn(std::string apn) { this->apn_ = apn; }
  void set_status_pin(gpio_num_t status_pin) { this->status_pin_ = status_pin; }
  void set_dtr_pin(gpio_num_t dtr_pin) { this->dtr_pin_ = dtr_pin; }
  void set_model(std::string model) {
    this->model_ = this->gsm_model_map_.count(model) ? gsm_model_map_[model] : GSMModel::UNKNOWN;
  }
  bool get_status() { return gpio_get_level(this->status_pin_); }

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
  GSMModel model_;
  std::unordered_map<std::string, GSMModel> gsm_model_map_ = {{"BG96", GSMModel::BG96},
                                                              {"SIM800", GSMModel::SIM800},
                                                              {"SIM7000", GSMModel::SIM7000},
                                                              {"SIM7070", GSMModel::SIM7070},
                                                              {"SIM7070_GNSS", GSMModel::SIM7070_GNSS},
                                                              {"SIM7600", GSMModel::SIM7600}};
  std::shared_ptr<esp_modem::DTE> dte;
  std::unique_ptr<esp_modem::DCE> dce;  // public ?
  esp_modem::esp_netif_t *ppp_netif{nullptr};
  esp_modem_dte_config_t dte_config;
  GSMComponentState state_{GSMComponentState::STOPPED};
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
extern GSMComponent *global_gsm_component;

}  // namespace gsm
}  // namespace esphome

#endif