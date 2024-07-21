#ifdef USE_ESP_IDF
#include "modem_component.h"
#include "helpers.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/components/network/util.h"

#include <esp_netif.h>
#include <esp_netif_ppp.h>
#include <esp_event.h>
#include <driver/gpio.h>
#include <lwip/dns.h>

#include <cxx_include/esp_modem_dte.hpp>
#include <esp_modem_config.h>
#include <cxx_include/esp_modem_api.hpp>

#include <cstring>
#include <iostream>

static const size_t CONFIG_MODEM_UART_RX_BUFFER_SIZE = 2048;
static const size_t CONFIG_MODEM_UART_TX_BUFFER_SIZE = 1024;
static const uint8_t CONFIG_MODEM_UART_EVENT_QUEUE_SIZE = 30;
static const size_t CONFIG_MODEM_UART_EVENT_TASK_STACK_SIZE = 2048;
static const uint8_t CONFIG_MODEM_UART_EVENT_TASK_PRIORITY = 5;

namespace esphome {
namespace modem {

using namespace esp_modem;

ModemComponent *global_modem_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

ModemComponent::ModemComponent() {
  assert(global_modem_component == nullptr);
  global_modem_component = this;
}

void ModemComponent::dump_config() { ESP_LOGCONFIG(TAG, "Config Modem:"); }

float ModemComponent::get_setup_priority() const { return setup_priority::WIFI; }

bool ModemComponent::can_proceed() { return this->is_connected(); }

network::IPAddresses ModemComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  esp_netif_ip_info_t ip;
  ESP_LOGV(TAG, "get_ip_addresses");
  esp_err_t err = esp_netif_get_ip_info(this->ppp_netif_, &ip);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "esp_netif_get_ip_info failed: %s", esp_err_to_name(err));
  } else {
    addresses[0] = network::IPAddress(&ip.ip);
  }
  return addresses;
}

std::string ModemComponent::get_use_address() const {
  // not usefull for a modem ?
  if (this->use_address_.empty()) {
    return App.get_name() + ".local";
  }
  return this->use_address_;
}

void ModemComponent::set_use_address(const std::string &use_address) { this->use_address_ = use_address; }

bool ModemComponent::is_connected() { return this->state_ == ModemComponentState::CONNECTED; }

void ModemComponent::setup() {
  ESP_LOGI(TAG, "Setting up Modem...");

  esp_err_t err;
  err = esp_netif_init();
  ESPHL_ERROR_CHECK(err, "PPP netif init error");
  err = esp_event_loop_create_default();
  ESPHL_ERROR_CHECK(err, "PPP event loop init error");

  esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();

  this->ppp_netif_ = esp_netif_new(&netif_ppp_config);
  assert(this->ppp_netif_);

  // err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &ModemComponent::ip_event_handler, nullptr,
  //                                           nullptr);
  err = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ModemComponent::ip_event_handler, nullptr);
  ESPHL_ERROR_CHECK(err, "IP event handler register error");

  this->reset_();

  ESP_LOGV(TAG, "Setup finished");
}

void ModemComponent::reset_() {
  // destroy previous dte/dce, and recreate them.
  // destroying them seems to be the only way to have a clear state after hang up, and be able to reconnect.

  this->dte_.reset();
  this->dce.reset();

  ESP_LOGV(TAG, "DTE setup");
  esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
  this->dte_config_ = dte_config;

  this->dte_config_.uart_config.tx_io_num = this->tx_pin_;
  this->dte_config_.uart_config.rx_io_num = this->rx_pin_;
  // this->dte_config_.uart_config.rts_io_num =  static_cast<gpio_num_t>( CONFIG_EXAMPLE_MODEM_UART_RTS_PIN);
  // this->dte_config_.uart_config.cts_io_num =  static_cast<gpio_num_t>( CONFIG_EXAMPLE_MODEM_UART_CTS_PIN);
  this->dte_config_.uart_config.rx_buffer_size = CONFIG_MODEM_UART_RX_BUFFER_SIZE;
  this->dte_config_.uart_config.tx_buffer_size = CONFIG_MODEM_UART_TX_BUFFER_SIZE;
  this->dte_config_.uart_config.event_queue_size = CONFIG_MODEM_UART_EVENT_QUEUE_SIZE;
  this->dte_config_.task_stack_size = CONFIG_MODEM_UART_EVENT_TASK_STACK_SIZE * 2;
  this->dte_config_.task_priority = CONFIG_MODEM_UART_EVENT_TASK_PRIORITY;
  this->dte_config_.dte_buffer_size = CONFIG_MODEM_UART_RX_BUFFER_SIZE / 2;

  this->dte_ = create_uart_dte(&this->dte_config_);

  ESP_LOGV(TAG, "PPP netif setup");

  assert(this->dte_);

  ESP_LOGV(TAG, "Set APN: %s", this->apn_.c_str());
  esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(this->apn_.c_str());

  if (!this->username_.empty()) {
    ESP_LOGV(TAG, "Set auth: username: %s password: %s", this->username_.c_str(), this->password_.c_str());
    ESPHL_ERROR_CHECK(esp_netif_ppp_set_auth(this->ppp_netif_, NETIF_PPP_AUTHTYPE_PAP, this->username_.c_str(),
                                             this->password_.c_str()),
                      "ppp set auth");
  }
  ESP_LOGV(TAG, "DCE setup");

  // NOLINTBEGIN(bugprone-branch-clone)
  // ( because create_modem_dce(dce_factory::ModemType, config, std::move(dte), netif) is private )
  switch (this->model_) {
    case ModemModel::BG96:
      this->dce = create_BG96_dce(&dce_config, this->dte_, this->ppp_netif_);
      break;
    case ModemModel::SIM800:
      this->dce = create_SIM800_dce(&dce_config, this->dte_, this->ppp_netif_);
      break;
    case ModemModel::SIM7000:
      this->dce = create_SIM7000_dce(&dce_config, this->dte_, this->ppp_netif_);
      break;
    case ModemModel::SIM7600:
      this->dce = create_SIM7600_dce(&dce_config, this->dte_, this->ppp_netif_);
      break;
    default:
      ESP_LOGE(TAG, "Unknown modem model");
      return;
      break;
  }
  // NOLINTEND(bugprone-branch-clone)

  assert(this->dce);

  if (this->dte_config_.uart_config.flow_control == ESP_MODEM_FLOW_CONTROL_HW) {
    if (command_result::OK != this->dce->set_flow_control(2, 2)) {
      ESP_LOGE(TAG, "Failed to set the set_flow_control mode");
      return;
    }
    ESP_LOGI(TAG, "set_flow_control OK");
  } else {
    ESP_LOGI(TAG, "not set_flow_control, because 2-wire mode active.");
  }

  if (this->enabled_ && !this->modem_ready()) {
    // if the esp has rebooted, but the modem not, it is still in cmux mode
    // So we close cmux.
    // The drawback is that if the modem is poweroff, those commands will take some time to execute.
    Watchdog wdt(60);
    this->dce->set_mode(esp_modem::modem_mode::CMUX_MANUAL_MODE);
    this->dce->set_mode(esp_modem::modem_mode::CMUX_MANUAL_COMMAND);
    this->dce->set_mode(esp_modem::modem_mode::CMUX_MANUAL_EXIT);
    if (!this->modem_ready()) {
      ESP_LOGW(TAG, "Modem still not ready after reset");
    } else {
      ESP_LOGD(TAG, "Modem exited previous CMUX session");
    }
  }
}

void ModemComponent::start_connect_() {
  Watchdog wdt(60);
  this->connect_begin_ = millis();
  this->status_set_warning("Starting connection");

  global_modem_component->got_ipv4_address_ = false;

  bool pin_ok = true;
  if (this->dce->read_pin(pin_ok) == command_result::OK && !pin_ok) {
    if (!this->pin_code_.empty()) {
      ESP_LOGV(TAG, "Set pin code: %s", this->pin_code_.c_str());
      this->dce->set_pin(this->pin_code_);
      delay(this->command_delay_);  // NOLINT
    }
    if (this->dce->read_pin(pin_ok) == command_result::OK && !pin_ok) {
      ESP_LOGE(TAG, "Invalid PIN");
      return;
    }
  }
  if (pin_ok) {
    if (this->pin_code_.empty()) {
      ESP_LOGD(TAG, "PIN not needed");
    } else {
      ESP_LOGD(TAG, "PIN unlocked");
    }
  }

  ESP_LOGD(TAG, "Entering CMUX mode");
  if (this->dce->set_mode(modem_mode::CMUX_MODE)) {
    ESP_LOGD(TAG, "Modem has correctly entered multiplexed command/data mode");
  } else {
    ESP_LOGE(TAG, "Failed to configure multiplexed command mode. Trying to continue...");
  }

  // send initial AT commands from yaml
  for (const auto &cmd : this->init_at_commands_) {
    std::string result = this->send_at(cmd);
    if (result == "ERROR") {
      ESP_LOGE(TAG, "Error while executing 'init_at' '%s' command", cmd.c_str());
    } else {
      ESP_LOGI(TAG, "'init_at' '%s' result: %s", cmd.c_str(), result.c_str());
    }
  }
}

void ModemComponent::ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  ip_event_got_ip_t *event;
  const esp_netif_ip_info_t *ip_info;
  switch (event_id) {
    case IP_EVENT_PPP_GOT_IP:
      event = (ip_event_got_ip_t *) event_data;
      ip_info = &event->ip_info;
      ESP_LOGD(TAG, "[IP event] Got IP " IPSTR, IP2STR(&ip_info->ip));
      global_modem_component->got_ipv4_address_ = true;
      global_modem_component->connected_ = true;
      break;
    case IP_EVENT_PPP_LOST_IP:
      ESP_LOGV(TAG, "[IP event] Lost IP");
      global_modem_component->got_ipv4_address_ = false;
      global_modem_component->connected_ = false;
      break;
  }
}

void ModemComponent::loop() {
  static ModemComponentState last_state = this->state_;
  const uint32_t now = millis();
  static uint32_t last_health_check = now;
  const uint32_t healh_check_interval = 30000;

  switch (this->state_) {
    case ModemComponentState::NOT_RESPONDING:
      if (this->start_) {
        if (this->modem_ready()) {
          ESP_LOGI(TAG, "Modem recovered");
          this->state_ = ModemComponentState::DISCONNECTED;
        } else {
          if (this->not_responding_cb_) {
            if (!this->not_responding_cb_->is_action_running()) {
              ESP_LOGD(TAG, "Calling 'on_not_responding' callback");
              this->not_responding_cb_->trigger();
            }
          } else {
            ESP_LOGW(TAG, "Modem not responding, and no 'on_not_responding' action defined");
          }
        }
      }
      break;

    case ModemComponentState::DISCONNECTED:
      if (this->enabled_) {
        if (this->start_) {
          if (this->modem_ready()) {
            ESP_LOGI(TAG, "Starting modem connection");
            this->state_ = ModemComponentState::CONNECTING;
            this->start_connect_();
          } else {
            this->state_ = ModemComponentState::NOT_RESPONDING;
          }
        } else {
          // when disconnected, we have to reset the dte and the dce
          this->reset_();
          this->start_ = true;
        }
      } else {
        this->state_ = ModemComponentState::DISABLED;
      }
      break;

    case ModemComponentState::CONNECTING:
      if (!this->start_) {
        ESP_LOGI(TAG, "Stopped modem connection");
        this->state_ = ModemComponentState::DISCONNECTED;
      } else if (this->connected_) {
        ESP_LOGI(TAG, "Connected via Modem");
        this->state_ = ModemComponentState::CONNECTED;

        this->dump_connect_params_();
        this->status_clear_warning();

      } else if (now - this->connect_begin_ > 45000) {
        ESP_LOGW(TAG, "Connecting via Modem failed! Re-connecting...");
        this->state_ = ModemComponentState::DISCONNECTED;
      }
      break;
    case ModemComponentState::CONNECTED:
      if (!this->start_) {
        this->state_ = ModemComponentState::DISCONNECTED;
      } else if (!this->connected_) {
        ESP_LOGW(TAG, "Connection via Modem lost! Re-connecting...");
        this->state_ = ModemComponentState::CONNECTING;
        this->start_connect_();
      } else {
        if ((now - last_health_check) >= healh_check_interval) {
          ESP_LOGV(TAG, "Health check");
          last_health_check = now;
          if (!this->modem_ready()) {
            ESP_LOGW(TAG, "Modem not responding while connected");
            this->state_ = ModemComponentState::NOT_RESPONDING;
          }
        }
      }
      break;

    case ModemComponentState::DISCONNECTING:
      if (this->start_) {
        if (this->connected_) {
          ESP_LOGD(TAG, "Hanging up...");
          this->dce->hang_up();
        }
        this->start_ = false;
      } else if (!this->connected_) {
        // ip lost as expected
        this->state_ = ModemComponentState::DISCONNECTED;
        this->reset_();  // reset dce/dte
      } else {
        // waiting for ip to be lost (TODO: possible infinite loop ?)
      }

      break;

    case ModemComponentState::DISABLED:
      if (this->enabled_) {
        this->state_ = ModemComponentState::DISCONNECTED;
      }
      break;
  }

  if (this->state_ != last_state) {
    ESP_LOGV(TAG, "State changed: %s -> %s", state_to_string(last_state).c_str(),
             state_to_string(this->state_).c_str());
    this->on_state_callback_.call(this->state_);

    last_state = this->state_;
  }
}

void ModemComponent::enable() {
  if (this->state_ == ModemComponentState::DISABLED) {
    this->state_ = ModemComponentState::DISCONNECTED;
  }
  this->start_ = true;
  this->enabled_ = true;
}

void ModemComponent::disable() {
  this->enabled_ = false;
  this->state_ = ModemComponentState::DISCONNECTING;
}

void ModemComponent::dump_connect_params_() {
  esp_netif_ip_info_t ip;
  esp_netif_get_ip_info(this->ppp_netif_, &ip);
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

std::string ModemComponent::send_at(const std::string &cmd) {
  std::string result;
  bool status;
  ESP_LOGV(TAG, "Sending command: %s", cmd.c_str());
  status = this->dce->at(cmd, result, this->command_delay_) == esp_modem::command_result::OK;
  ESP_LOGV(TAG, "Result for command %s: %s (status %d)", cmd.c_str(), result.c_str(), status);
  if (!status) {
    result = "ERROR";
  }
  return result;
}

bool ModemComponent::get_imei(std::string &result) {
  // wrapper around this->dce->get_imei() that check that the result is valid
  // (so it can be used to check if the modem is responding correctly (a simple 'AT' cmd is sometime not enough))
  command_result status;
  status = this->dce->get_imei(result);
  bool success = true;

  if (status == command_result::OK && result.length() == 15) {
    for (char c : result) {
      if (!isdigit(static_cast<unsigned char>(c))) {
        success = false;
        break;
      }
    }
  } else {
    success = false;
  }

  if (!success) {
    result = "UNAVAILABLE";
  }
  return success;
}

bool ModemComponent::modem_ready() {
  // check if the modem is ready to answer AT commands
  std::string imei;
  bool status;
  {
    // Temp increase watchdog timout
    Watchdog wdt(60);
    status = this->get_imei(imei);
  }
  return status;
}

void ModemComponent::add_on_state_callback(std::function<void(ModemComponentState)> &&callback) {
  this->on_state_callback_.add(std::move(callback));
}

}  // namespace modem
}  // namespace esphome

#endif
