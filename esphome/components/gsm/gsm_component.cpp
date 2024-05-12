#if defined(USE_ESP32) || defined(USE_ESP_IDF)
#include "gsm_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/components/network/util.h"
#include <esp_netif.h>
#include <esp_netif_ppp.h>
#include <esp_log.h>
#include <esp_event.h>
#include <cxx_include/esp_modem_dte.hpp>
#include <esp_modem_config.h>
#include <cxx_include/esp_modem_api.hpp>
#include <driver/gpio.h>
#include <lwip/dns.h>
#include <cstring>
#include <iostream>
#include "sdkconfig.h"

static const size_t CONFIG_MODEM_UART_RX_BUFFER_SIZE = 1024;
static const size_t CONFIG_MODEM_UART_TX_BUFFER_SIZE = 512;
static const uint8_t CONFIG_MODEM_UART_EVENT_QUEUE_SIZE = 30;
static const size_t CONFIG_MODEM_UART_EVENT_TASK_STACK_SIZE = 2048;
static const uint8_t CONFIG_MODEM_UART_EVENT_TASK_PRIORITY = 5;

namespace esphome {
namespace gsm {

GSMComponent *global_gsm_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#define ESPHL_ERROR_CHECK(err, message) \
  if ((err) != ESP_OK) { \
    ESP_LOGE(TAG, message ": (%d) %s", err, esp_err_to_name(err)); \
    this->mark_failed(); \
    return; \
  }

using namespace esp_modem;

GSMComponent::GSMComponent() { global_gsm_component = this; }

void GSMComponent::dump_config() { ESP_LOGCONFIG(TAG, "Config GSM:"); }

float GSMComponent::get_setup_priority() const { return setup_priority::WIFI; }

bool GSMComponent::can_proceed() { return this->is_connected(); }

network::IPAddresses GSMComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  esp_netif_ip_info_t ip;
  ESP_LOGV(TAG, "get_ip_addresses");
  esp_err_t err = esp_netif_get_ip_info(this->ppp_netif, &ip);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_netif_get_ip_info failed: %s", esp_err_to_name(err));
    // TODO: do something smarter
    // return false;
  } else {
    addresses[0] = network::IPAddress(&ip.ip);
  }
  return addresses;
}

std::string GSMComponent::get_use_address() const {
  if (this->use_address_.empty()) {
    return App.get_name() + ".local";
  }
  return this->use_address_;
}

void GSMComponent::set_use_address(const std::string &use_address) { this->use_address_ = use_address; }

bool GSMComponent::is_connected() { return this->state_ == GSMComponentState::CONNECTED; }

void GSMComponent::setup() {
  ESP_LOGI(TAG, "Setting up GSM...");

  this->config_gpio_();

  ESP_LOGI(TAG, "Status: %d", (int) this->get_status());

  // to be sure the modem is not allready connected
  // this->powerdown();

  ESP_LOGV(TAG, "DTE setup");
  esp_modem_dte_config_t dte_config_ = ESP_MODEM_DTE_DEFAULT_CONFIG();
  this->dte_config = dte_config_;

  // this->dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
  this->dte_config.uart_config.tx_io_num = this->tx_pin_;
  this->dte_config.uart_config.rx_io_num = this->rx_pin_;
  // this->dte_config.uart_config.rts_io_num =  static_cast<gpio_num_t>( CONFIG_EXAMPLE_MODEM_UART_RTS_PIN);
  // this->dte_config.uart_config.cts_io_num =  static_cast<gpio_num_t>( CONFIG_EXAMPLE_MODEM_UART_CTS_PIN);
  this->dte_config.uart_config.rx_buffer_size = CONFIG_MODEM_UART_RX_BUFFER_SIZE;
  this->dte_config.uart_config.tx_buffer_size = CONFIG_MODEM_UART_TX_BUFFER_SIZE;
  this->dte_config.uart_config.event_queue_size = CONFIG_MODEM_UART_EVENT_QUEUE_SIZE;
  this->dte_config.task_stack_size = CONFIG_MODEM_UART_EVENT_TASK_STACK_SIZE * 2;
  this->dte_config.task_priority = CONFIG_MODEM_UART_EVENT_TASK_PRIORITY;
  this->dte_config.dte_buffer_size = CONFIG_MODEM_UART_RX_BUFFER_SIZE / 2;

  this->dte = esp_modem::create_uart_dte(&this->dte_config);

  assert(this->dte);

  ESP_LOGV(TAG, "Set APN: %s", this->apn_.c_str());
  esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(this->apn_.c_str());

  ESP_LOGV(TAG, "PPP netif setup");
  esp_err_t err;
  err = esp_netif_init();
  ESPHL_ERROR_CHECK(err, "PPP netif init error");
  err = esp_event_loop_create_default();
  ESPHL_ERROR_CHECK(err, "PPP event loop init error");
  esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();

  this->ppp_netif = esp_netif_new(&netif_ppp_config);
  assert(this->ppp_netif);
  if (!this->username_.empty()) {
    ESP_LOGV(TAG, "Set auth: username: %s password: %s", this->username_.c_str(), this->password_.c_str());
    ESPHL_ERROR_CHECK(esp_netif_ppp_set_auth(this->ppp_netif, NETIF_PPP_AUTHTYPE_PAP, this->username_.c_str(),
                                             this->password_.c_str()),
                      "ppp set auth");
  }
  // dns setup not needed (perhaps fallback ?)
  // esp_netif_dns_info_t dns_main = {};
  // dns_main.ip.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8");
  // dns_main.ip.type = ESP_IPADDR_TYPE_V4;
  // ESPHL_ERROR_CHECK(esp_netif_set_dns_info(this->ppp_netif, ESP_NETIF_DNS_MAIN, &dns_main), "dns_main");

  // Register user defined event handers
  err = esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, &GSMComponent::got_ip_event_handler, nullptr);
  ESPHL_ERROR_CHECK(err, "GOT IP event handler register error");

  ESP_LOGV(TAG, "DCE setup");

  switch (this->model_) {
    case GSMModel::BG96:
      this->dce = create_BG96_dce(&dce_config, dte, this->ppp_netif);
      break;
    case GSMModel::SIM800:
      this->dce = create_SIM800_dce(&dce_config, dte, this->ppp_netif);
      break;
    case GSMModel::SIM7000:
      this->dce = create_SIM7000_dce(&dce_config, dte, this->ppp_netif);
      break;
    case GSMModel::SIM7600:
      this->dce = create_SIM7600_dce(&dce_config, dte, this->ppp_netif);
      break;
    default:
      ESP_LOGE(TAG, "Unknown model");
      return;
      break;
  }

  assert(this->dce);

  this->started_ = true;
  ESP_LOGV(TAG, "Setup finished");
}

void GSMComponent::start_connect_() {
  this->connect_begin_ = millis();
  this->status_set_warning();

  ESP_LOGI(TAG, "Status: %d", (int) this->get_status());

  this->poweron();

  ESP_LOGI(TAG, "Status: %d", (int) this->get_status());

  // esp_err_t err;
  // err = esp_netif_set_hostname(this->ppp_netif, App.get_name().c_str());
  // if (err != ERR_OK) {
  //   ESP_LOGW(TAG, "esp_netif_set_hostname failed: %s", esp_err_to_name(err));
  // }

  global_gsm_component->got_ipv4_address_ = false;  // why not this ?

  this->dce->set_mode(esp_modem::modem_mode::CMUX_MANUAL_COMMAND);
  vTaskDelay(pdMS_TO_TICKS(2000));

  command_result res = command_result::TIMEOUT;

  int retry = 0;
  while (res != command_result::OK) {
    res = this->dce->sync();
    if (res != command_result::OK) {
      ESP_LOGW(TAG, "modem not responding");
      ESP_LOGI(TAG, "Status: %d", (int) this->get_status());
      this->dce->set_command_mode();
      App.feed_wdt();
      vTaskDelay(pdMS_TO_TICKS(7000));
    }
    retry++;
    if (retry > 10)
      break;
  }

  if (res != command_result::OK) {
    ESP_LOGW(TAG, "Unable to sync modem. Will retry later");
    this->powerdown();
    return;
  }

  if (this->dte_config.uart_config.flow_control == ESP_MODEM_FLOW_CONTROL_HW) {
    if (command_result::OK != dce->set_flow_control(2, 2)) {
      ESP_LOGE(TAG, "Failed to set the set_flow_control mode");
      return;
    }
    ESP_LOGI(TAG, "set_flow_control OK");
  } else {
    ESP_LOGI(TAG, "not set_flow_control, because 2-wire mode active.");
  }

  /* Setup basic operation mode for the DCE (pin if used, CMUX mode) */
  if (!this->pin_code_.empty()) {
    bool pin_ok = true;
    ESP_LOGV(TAG, "Set pin code: %s", this->pin_code_.c_str());
    if (dce->read_pin(pin_ok) == command_result::OK && !pin_ok) {
      ESP_MODEM_THROW_IF_FALSE(dce->set_pin(this->pin_code_) == command_result::OK, "Cannot set PIN!");
      vTaskDelay(pdMS_TO_TICKS(2000));  // Need to wait for some time after unlocking the SIM
    }
  }

  ESP_LOGD(TAG, "Entering CMUX mode");

  vTaskDelay(pdMS_TO_TICKS(2000));

  if (this->dce->set_mode(esp_modem::modem_mode::CMUX_MODE)) {
    ESP_LOGD(TAG, "Modem has correctly entered multiplexed command/data mode");
  } else {
    ESP_LOGE(TAG, "Failed to configure multiplexed command mode... exiting");
    return;
  }
  vTaskDelay(pdMS_TO_TICKS(2000));
}

void GSMComponent::got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
  const esp_netif_ip_info_t *ip_info = &event->ip_info;
  ESP_LOGW(TAG, "[IP event] Got IP " IPSTR, IP2STR(&ip_info->ip));
  vTaskDelay(pdMS_TO_TICKS(1000));  // FIXME tmp
  global_gsm_component->got_ipv4_address_ = true;
  global_gsm_component->connected_ = true;
}

void GSMComponent::loop() {
  const uint32_t now = millis();

  switch (this->state_) {
    case GSMComponentState::STOPPED:
      if (this->started_) {
        ESP_LOGI(TAG, "Starting gsm connection");
        this->state_ = GSMComponentState::CONNECTING;
        this->start_connect_();
      }
      break;
    case GSMComponentState::CONNECTING:
      if (!this->started_) {
        ESP_LOGI(TAG, "Stopped ethernet connection");
        this->state_ = GSMComponentState::STOPPED;
      } else if (this->connected_) {
        // connection established
        ESP_LOGI(TAG, "Connected via GSM");
        this->state_ = GSMComponentState::CONNECTED;

        this->dump_connect_params_();
        this->status_clear_warning();
      } else if (now - this->connect_begin_ > 45000) {
        ESP_LOGW(TAG, "Connecting via GSM failed! Re-connecting...");
        this->start_connect_();
      }
      break;
    case GSMComponentState::CONNECTED:
      if (!this->started_) {
        ESP_LOGI(TAG, "Stopped GSM connection");
        this->state_ = GSMComponentState::STOPPED;
      } else if (!this->connected_) {
        ESP_LOGW(TAG, "Connection via GSM lost! Re-connecting...");
        this->state_ = GSMComponentState::CONNECTING;
        this->start_connect_();
      }
      break;
  }
}

void GSMComponent::dump_connect_params_() {
  esp_netif_ip_info_t ip;
  esp_netif_get_ip_info(this->ppp_netif, &ip);
  ESP_LOGCONFIG(TAG, "  IP Address: %s", network::IPAddress(&ip.ip).str().c_str());
  ESP_LOGCONFIG(TAG, "  Hostname: '%s'", App.get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Subnet: %s", network::IPAddress(&ip.netmask).str().c_str());
  ESP_LOGCONFIG(TAG, "  Gateway: %s", network::IPAddress(&ip.gw).str().c_str());

  const ip_addr_t *dns_main_ip = dns_getserver(ESP_NETIF_DNS_MAIN);
  const ip_addr_t *dns_backup_ip = dns_getserver(ESP_NETIF_DNS_BACKUP);
  const ip_addr_t *dns_fallback_ip = dns_getserver(ESP_NETIF_DNS_FALLBACK);

  ESP_LOGCONFIG(TAG, "  DNS main: %s", network::IPAddress(dns_main_ip).str().c_str());
  ESP_LOGCONFIG(TAG, "  DNS backup: %s", network::IPAddress(dns_backup_ip).str().c_str());
  ESP_LOGCONFIG(TAG, "  DNS fallback: %s", network::IPAddress(dns_fallback_ip).str().c_str());
}

void GSMComponent::config_gpio_() {
  ESP_LOGV(TAG, "Configuring GPIOs...");
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = 0ULL;
  if (this->power_pin_ != gpio_num_t::GPIO_NUM_NC) {
    io_conf.pin_bit_mask = io_conf.pin_bit_mask | (1ULL << this->power_pin_);
  }
  if (this->flight_pin_ != gpio_num_t::GPIO_NUM_NC) {
    io_conf.pin_bit_mask = io_conf.pin_bit_mask | (1ULL << this->flight_pin_);
  }
  if (this->dtr_pin_ != gpio_num_t::GPIO_NUM_NC) {
    io_conf.pin_bit_mask = io_conf.pin_bit_mask | (1ULL << this->dtr_pin_);
  }
  // io_conf.pin_bit_mask = ((1ULL << this->power_pin_) | (1ULL << this->flight_pin_));

  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

  gpio_config(&io_conf);

  io_conf.pin_bit_mask = 0ULL;
  if (this->status_pin_ != gpio_num_t::GPIO_NUM_NC) {
    io_conf.pin_bit_mask = io_conf.pin_bit_mask | (1ULL << this->status_pin_);
  }
  io_conf.mode = GPIO_MODE_INPUT;
  gpio_config(&io_conf);
}

void GSMComponent::poweron(void) {
  /* Power on the modem */

  ESP_LOGI(TAG, "Status: %d", (int) this->get_status());

  ESP_LOGI(TAG, "Power on  modem");
  // https://github.com/Xinyuan-LilyGO/LilyGO-T-SIM7000G/issues/251
  if (this->power_pin_ != gpio_num_t::GPIO_NUM_NC) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_ERROR_CHECK(gpio_set_level(this->power_pin_, 0));  // low = on, high = off
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(gpio_set_level(this->power_pin_, 1));
    vTaskDelay(pdMS_TO_TICKS(1010));
    ESP_ERROR_CHECK(gpio_set_level(this->power_pin_, 0));
    vTaskDelay(pdMS_TO_TICKS(4050));  // Ton uart 4.5sec but seems to need ~7sec after hard (button) reset
  } else {
    ESP_LOGW(TAG, "No power pin defined. Trying to continue");
  }

  if (this->flight_pin_ != gpio_num_t::GPIO_NUM_NC) {
    ESP_ERROR_CHECK(gpio_set_level(this->flight_pin_, 1));  // need to be high
  } else {
    ESP_LOGW(TAG, "No flight pin defined. Trying to continue");
  }
  vTaskDelay(pdMS_TO_TICKS(15000));
  App.feed_wdt();
}

void GSMComponent::powerdown(void) {
  ESP_LOGI(TAG, "Power down modem");
  ESP_ERROR_CHECK(gpio_set_level(this->power_pin_, 1));
}

}  // namespace gsm
}  // namespace esphome

#endif
