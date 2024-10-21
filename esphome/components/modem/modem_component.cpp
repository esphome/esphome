#ifdef USE_ESP32
#ifdef USE_ESP_IDF

#include "modem_component.h"

#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/application.h"

#include "esp_modem_c_api_types.h"
#include "esp_netif_ppp.h"
#include "cxx_include/esp_modem_types.hpp"

#include <cinttypes>
#include "driver/gpio.h"
#include <lwip/dns.h>
#include "esp_event.h"

namespace esphome {
namespace modem {

static const char *const TAG = "modem";

ModemComponent *global_modem_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#define ESPHL_ERROR_CHECK(err, message) \
  if ((err) != ESP_OK) { \
    ESP_LOGE(TAG, message ": (%d) %s", err, esp_err_to_name(err)); \
    this->mark_failed(); \
    return; \
  }

ModemComponent::ModemComponent() { global_modem_component = this; }

// setup
void ModemComponent::setup() {
  // esp_log_level_set("esp-netif_lwip-ppp", ESP_LOG_VERBOSE);
  // esp_log_level_set("esp-netif_lwip", ESP_LOG_VERBOSE);
  // esp_log_level_set("modem", ESP_LOG_VERBOSE);
  // esp_log_level_set("mqtt", ESP_LOG_VERBOSE);
  // esp_log_level_set("command_lib", ESP_LOG_VERBOSE);
  // esp_log_level_set("uart_terminal", ESP_LOG_VERBOSE);
  // esp_log_level_set("vfs_socket_creator", ESP_LOG_VERBOSE);
  // esp_log_level_set("vfs_uart_creator", ESP_LOG_VERBOSE);
  // esp_log_level_set("fs_terminal", ESP_LOG_VERBOSE);

  ESP_LOGCONFIG(TAG, "Setting up modem...");
  if (this->power_pin_) {
    this->power_pin_->setup();
  }
  if (this->pwrkey_pin_) {
    this->pwrkey_pin_->setup();
  }
  if (this->reset_pin_) {
    this->reset_pin_->setup();
  }
  if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
    // Delay here to allow power to stabilise before Modem is initialized.
    delay(300);  // NOLINT
  }

  esp_err_t err;
  err = esp_netif_init();
  ESPHL_ERROR_CHECK(err, "modem netif init error");
  err = esp_event_loop_create_default();
  ESPHL_ERROR_CHECK(err, "modem event loop error");
  ESP_LOGCONFIG(TAG, "Initing netif");
  esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ModemComponent::got_ip_event_handler, nullptr);
  ESP_LOGD(TAG, "Initializing esp_modem");
  this->modem_netif_init_();
  this->dte_init_();

  this->started_ = true;
}

void ModemComponent::loop() {
  const int now = millis();
  if (!ModemComponent::check_modem_component_state_timings_()) {
    return;
  }
  switch (this->state_) {
    // The state of the beginning of power supply to the modem
    case ModemComponentState::TURNING_ON_POWER:
      if (power_pin_) {
        this->power_pin_->digital_write(true);
        ESP_LOGD(TAG, "Modem turn on");
        if (this->pwrkey_pin_) {
          this->set_state_(ModemComponentState::TURNING_ON_PWRKEY);
        } else {
          this->set_state_(ModemComponentState::SYNC);
        }
      } else {
        ESP_LOGD(TAG, "Can't turn on modem power pin because it is not configured, go to turn on pwrkey");
        this->set_state_(ModemComponentState::TURNING_ON_RESET);
      }
      break;

    // Modem power supply end state
    case ModemComponentState::TURNING_OFF_POWER:
      this->power_pin_->digital_write(false);
      ESP_LOGD(TAG, "modem turn off");
      this->set_state_(ModemComponentState::TURNING_ON_POWER);
      break;

    // The state holds the power key
    case ModemComponentState::TURNING_ON_PWRKEY:
      if (pwrkey_pin_) {
        this->pwrkey_pin_->digital_write(false);
        ESP_LOGD(TAG, "pwrkey turn on");
        this->set_state_(ModemComponentState::TURNING_OFF_PWRKEY);
      } else {
        ESP_LOGD(TAG, "Can't turn on pwrkey pin because it is not configured, go to reset power modem");
        this->set_state_(ModemComponentState::TURNING_ON_POWER);
        break;
      }
      break;

    // The state releases the power key
    case ModemComponentState::TURNING_OFF_PWRKEY:
      this->pwrkey_pin_->digital_write(true);
      ESP_LOGD(TAG, "pwrkey turn off");
      this->set_state_(ModemComponentState::SYNC);
      break;

    // The state of the beginning of the reset of the modem
    case ModemComponentState::TURNING_ON_RESET:
      if (reset_pin_) {
        this->reset_pin_->digital_write(false);
        ESP_LOGD(TAG, "turn on reset");
        this->set_state_(ModemComponentState::TURNING_OFF_RESET);
      } else {
        ESP_LOGD(TAG, "Can't turn on reset pin because it is not configured, go to turn on pwkey");
        this->set_state_(ModemComponentState::TURNING_OFF_POWER);
        break;
      }
      break;

    // The state of the end of the reset of the modem
    case ModemComponentState::TURNING_OFF_RESET:
      this->reset_pin_->digital_write(true);
      ESP_LOGD(TAG, "turn off reset");
      this->set_state_(ModemComponentState::SYNC);
      break;

    // The state of waiting for the modem to connect, response to "AT" "OK"
    case ModemComponentState::SYNC:
      if (this->dce_->sync() == esp_modem::command_result::OK) {
        ESP_LOGD(TAG, "sync OK");
        this->set_state_(ModemComponentState::REGISTRATION_IN_NETWORK);
      } else {
        ESP_LOGD(TAG, "Wait sync");
      }

      break;

    // The state of waiting for the modem to register in the network
    case ModemComponentState::REGISTRATION_IN_NETWORK:
      if (get_rssi_()) {
        ESP_LOGD(TAG, "Starting modem connection");
        ESP_LOGD(TAG, "SIgnal quality: rssi=%d", get_rssi_());
        this->set_state_(ModemComponentState::CONNECTING);
        this->dce_->set_data();
      } else {
        ESP_LOGD(TAG, "Wait RSSI");
      }

      break;

    // The state of waiting state for receiving IP address
    case ModemComponentState::CONNECTING:
      ESP_LOGD(TAG, "Wait WAN");
      break;

    // The state of network connection established
    case ModemComponentState::CONNECTED:
      if (esp_netif_is_netif_up(this->modem_netif_)) {
        ESP_LOGD(TAG, "esp_netif_is_netif_UP");
      } else {
        ESP_LOGD(TAG, "esp_netif_is_netif_DOWN");
      }
      break;

    default:
      break;
  }
}

void ModemComponent::modem_netif_init_() {
  esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
  this->modem_netif_ = esp_netif_new(&netif_ppp_config);
  assert(this->modem_netif_);
  ESP_LOGD(TAG, "netif create succes");
}

void ModemComponent::dte_init_() {
  esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
  /* setup UART specific configuration based on kconfig options */
  dte_config.uart_config.tx_io_num = this->tx_pin_;
  dte_config.uart_config.rx_io_num = this->rx_pin_;
  dte_config.uart_config.rx_buffer_size = this->uart_rx_buffer_size_;
  dte_config.uart_config.tx_buffer_size = this->uart_tx_buffer_size_;
  dte_config.uart_config.event_queue_size = this->uart_event_queue_size_;
  dte_config.task_stack_size = this->uart_event_task_stack_size_;
  dte_config.task_priority = this->uart_event_task_priority_;
  dte_config.dte_buffer_size = this->uart_rx_buffer_size_ / 2;
  this->dte_ = esp_modem::create_uart_dte(&dte_config);
}

void ModemComponent::dce_init_() {
  esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(this->apn_.c_str());
  this->dce_ = esp_modem::create_SIM800_dce(&dce_config, dte_, this->modem_netif_);
}

bool ModemComponent::check_modem_component_state_timings_() {
  const int now = millis();
  ModemComponentStateTiming timing = this->modem_component_state_timing_map_[this->state_];
  if (timing.time_limit && ((this->change_state_ + timing.time_limit) < now)) {
    ESP_LOGE(TAG, "State time limit %s", this->state_to_string_(this->state_));
    this->set_state_(ModemComponentState::TURNING_ON_RESET);
    return true;
  }
  if (!timing.poll_period) {
    return true;
  }
  if ((this->pull_time_ + timing.poll_period) < now) {
    // ESP_LOGD(TAG, "it's time for pull");//%d %d", timing.poll_period, timing.time_limit);
    this->pull_time_ = now;
    return true;
  }
  return false;
}

void ModemComponent::set_state_(ModemComponentState state) {
  // execute before transition to state
  switch (state) {
    case ModemComponentState::SYNC:
      this->dce_init_();
      break;

    default:
      break;
  }
  ESP_LOGCONFIG(TAG, "Modem component change state from %s to %s", this->state_to_string_(this->state_),
                this->state_to_string_(state));
  this->state_ = state;
  this->change_state_ = millis();
}

const char *ModemComponent::state_to_string_(ModemComponentState state) {
  switch (state) {
    case ModemComponentState::TURNING_ON_POWER:
      return "TURNING_ON_POWER";
    case ModemComponentState::TURNING_OFF_POWER:
      return "TURNING_OFF_POWER";
    case ModemComponentState::TURNING_ON_PWRKEY:
      return "TURNING_ON_PWRKEY";
    case ModemComponentState::TURNING_OFF_PWRKEY:
      return "TURNING_OFF_PWRKEY";
    case ModemComponentState::TURNING_ON_RESET:
      return "TURNING_ON_RESET";
    case ModemComponentState::TURNING_OFF_RESET:
      return "TURNING_OFF_RESET";
    case ModemComponentState::SYNC:
      return "SYNC";
    case ModemComponentState::REGISTRATION_IN_NETWORK:
      return "REGISTRATION_IN_NETWORK";
    case ModemComponentState::CONNECTING:
      return "CONNECTING";
    case ModemComponentState::CONNECTED:
      return "CONNECTED";
  }
  return "UNKNOWN";
}

void ModemComponent::dump_config() {
  this->dump_connect_params();
  ESP_LOGCONFIG(TAG, "Modem:");
  ESP_LOGCONFIG(TAG, "  Type: %d", this->type_);
  ESP_LOGCONFIG(TAG, "  Power pin : %s", (this->power_pin_) ? this->power_pin_->dump_summary().c_str() : "Not defined");
  ESP_LOGCONFIG(TAG, "  Reset pin : %s", (this->reset_pin_) ? this->reset_pin_->dump_summary().c_str() : "Not defined");
  ESP_LOGCONFIG(TAG, "  Pwrkey pin : %s",
                (this->pwrkey_pin_) ? this->pwrkey_pin_->dump_summary().c_str() : "Not defined");
  ESP_LOGCONFIG(TAG, "  APN: %s", this->apn_.c_str());
  ESP_LOGCONFIG(TAG, "  TX Pin: %d", this->tx_pin_);
  ESP_LOGCONFIG(TAG, "  RX Pin: %d", this->rx_pin_);
  ESP_LOGCONFIG(TAG, "  UART Event Task Stack Size: %d", this->uart_event_task_stack_size_);
  ESP_LOGCONFIG(TAG, "  UART Event Task Priority: %d", this->uart_event_task_priority_);
  ESP_LOGCONFIG(TAG, "  UART Event Queue Size: %d", this->uart_event_queue_size_);
  ESP_LOGCONFIG(TAG, "  UART TX Buffer Size: %d", this->uart_tx_buffer_size_);
  ESP_LOGCONFIG(TAG, "  UART RX Buffer Size: %d", this->uart_rx_buffer_size_);
}

void ModemComponent::dump_connect_params() {
  esp_netif_ip_info_t ip;
  esp_netif_get_ip_info(this->modem_netif_, &ip);
  ESP_LOGCONFIG(TAG, "  IP Address: %s", network::IPAddress(&ip.ip).str().c_str());
  ESP_LOGCONFIG(TAG, "  Netmask: %s", network::IPAddress(&ip.netmask).str().c_str());
  ESP_LOGCONFIG(TAG, "  Gateway: %s", network::IPAddress(&ip.gw).str().c_str());
  esp_netif_dns_info_t dns_info;
  esp_netif_get_dns_info(this->modem_netif_, ESP_NETIF_DNS_MAIN, &dns_info);
  ESP_LOGCONFIG(TAG, "  DNS1: %s", network::IPAddress(&dns_info.ip.u_addr.ip4).str().c_str());
  esp_netif_get_dns_info(this->modem_netif_, ESP_NETIF_DNS_BACKUP, &dns_info);
  ESP_LOGCONFIG(TAG, "  DNS2: %s", network::IPAddress(&dns_info.ip.u_addr.ip4).str().c_str());
}

int ModemComponent::get_rssi_() {
  int rssi = 0, ber = 0;
  esp_modem::command_result errr = this->dce_->get_signal_quality(rssi, ber);
  // esp_err_t err = esp_modem::esp_modem_get_signal_quality(dce, &rssi, &ber);
  if (errr != esp_modem::command_result::OK) {
    ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with");
  }
  return rssi;
}

float ModemComponent::get_setup_priority() const { return setup_priority::MODEM; }

bool ModemComponent::can_proceed() { return this->is_connected(); }

network::IPAddress ModemComponent::get_ip_address() {
  esp_netif_ip_info_t ip;
  esp_netif_get_ip_info(this->modem_netif_, &ip);
  return network::IPAddress(&ip.ip);
}

void ModemComponent::got_ip_event_handler(void *arg, esp_event_base_t event_base, int event_id, void *event_data) {
  ESP_LOGD(TAG, "IP event! %" PRIu32, event_id);
  if (event_id == IP_EVENT_PPP_GOT_IP) {
    global_modem_component->set_state_(ModemComponentState::CONNECTED);
    esp_netif_dns_info_t dns_info;

    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    esp_netif_t *netif = event->esp_netif;

    ESP_LOGI(TAG, "Modem Connect to PPP Server");
    ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
    ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
    ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
    esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
    ESP_LOGI(TAG, "DNS 1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
    ESP_LOGI(TAG, "DNS 2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    ESP_LOGI(TAG, "~~~~~~~~~~~~~~");

    ESP_LOGD(TAG, "GOT ip event!!!");
  } else if (event_id == IP_EVENT_PPP_LOST_IP) {
    ESP_LOGD(TAG, "Modem Disconnect from PPP Server");
    global_modem_component->set_state_(ModemComponentState::TURNING_ON_RESET);
  }
}

bool ModemComponent::is_connected() { return this->state_ == ModemComponentState::CONNECTED; }
void ModemComponent::set_power_pin(InternalGPIOPin *power_pin) { this->power_pin_ = power_pin; }
void ModemComponent::set_pwrkey_pin(InternalGPIOPin *pwrkey_pin) { this->pwrkey_pin_ = pwrkey_pin; }
void ModemComponent::set_type(ModemType type) { this->type_ = type; }
void ModemComponent::set_reset_pin(InternalGPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
void ModemComponent::set_apn(const std::string &apn) { this->apn_ = apn; }
void ModemComponent::set_tx_pin(uint8_t tx_pin) { this->tx_pin_ = tx_pin; }
void ModemComponent::set_rx_pin(uint8_t rx_pin) { this->rx_pin_ = rx_pin; }
void ModemComponent::set_uart_event_task_stack_size(int uart_event_task_stack_size) {
  this->uart_event_task_stack_size_ = uart_event_task_stack_size;
}
void ModemComponent::set_uart_event_task_priority(int uart_event_task_priority) {
  this->uart_event_task_priority_ = uart_event_task_priority;
}
void ModemComponent::set_uart_event_queue_size(int uart_event_queue_size) {
  this->uart_event_queue_size_ = uart_event_queue_size;
}
void ModemComponent::set_uart_tx_buffer_size(int uart_tx_buffer_size) {
  this->uart_tx_buffer_size_ = uart_tx_buffer_size;
}
void ModemComponent::set_uart_rx_buffer_size(int uart_rx_buffer_size) {
  this->uart_rx_buffer_size_ = uart_rx_buffer_size;
}

std::string ModemComponent::get_use_address() const {
  if (this->use_address_.empty()) {
    return App.get_name() + ".local";
  }
  return this->use_address_;
}

void ModemComponent::set_use_address(const std::string &use_address) { this->use_address_ = use_address; }

}  // namespace modem
}  // namespace esphome

#endif  // USE_ESP_IDF
#endif  // USE_ESP32
