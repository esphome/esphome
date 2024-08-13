#ifdef USE_ESP_IDF
#include "modem_component.h"
#include "helpers.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/components/network/util.h"
#include "esphome/components/watchdog/watchdog.h"

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
#include <cmath>

#define ESPHL_ERROR_CHECK(err, message) \
  if ((err) != ESP_OK) { \
    ESP_LOGE(TAG, message ": (%d) %s", err, esp_err_to_name(err)); \
    this->mark_failed(); \
    return; \
  }

#define ESPMODEM_ERROR_CHECK(err, message) \
  if ((err) != command_result::OK) { \
    ESP_LOGE(TAG, message ": %s", command_result_to_string(err).c_str()); \
  }

namespace esphome {
namespace modem {

using namespace esp_modem;

static const char *const TAG = "modem";

ModemComponent *global_modem_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

ModemComponent::ModemComponent() {
  assert(global_modem_component == nullptr);
  global_modem_component = this;
}

void ModemComponent::enable_debug() {
  esp_log_level_set("command_lib", ESP_LOG_VERBOSE);
  // esp_log_level_set("CMUX", ESP_LOG_VERBOSE);
  // esp_log_level_set("CMUX Received", ESP_LOG_VERBOSE);
}

AtCommandResult ModemComponent::send_at(const std::string &cmd, uint32_t timeout) {
  AtCommandResult at_command_result;
  at_command_result.success = false;
  command_result status = command_result::FAIL;
  if (this->modem_ready()) {
    ESP_LOGV(TAG, "Sending command: %s", cmd.c_str());
    status = this->dce->at(cmd, at_command_result.result, timeout);
    ESP_LOGD(TAG, "Result for command %s: %s (status %s)", cmd.c_str(), at_command_result.result.c_str(),
             command_result_to_string(status).c_str());
  }
  if (status == command_result::OK) {
    at_command_result.success = true;
  }
  return at_command_result;
}

AtCommandResult ModemComponent::send_at(const std::string &cmd) { return this->send_at(cmd, this->command_delay_); }

AtCommandResult ModemComponent::get_imei() {
  // get the imei, and check the result is a valid imei string
  // (so it can be used to check if the modem is responding correctly (a simple 'AT' cmd is sometime not enough))
  AtCommandResult at_command_result;
  at_command_result.success = false;
  command_result status = command_result::FAIL;
  status = this->dce->at("AT+CGSN", at_command_result.result, 1000);
  if ((status == command_result::OK) && at_command_result.result.length() == 15) {
    at_command_result.success = true;
    for (char c : at_command_result.result) {
      if (!isdigit(static_cast<unsigned char>(c))) {
        at_command_result.success = false;
        break;
      }
    }
  } else {
    at_command_result.success = false;
  }
  return at_command_result;
}

bool ModemComponent::get_power_status() {
#ifdef USE_MODEM_STATUS
  // This code is not fully checked. The status pin seems to be flickering on Lilygo T-SIM7600
  return this->status_pin_->digital_read();
#else
  if (!this->cmux_ && this->internal_state_.connected) {
    // Data mode, connected:  assume power is OK
    return true;
  }
  return this->modem_ready();
#endif
}

bool ModemComponent::modem_ready(bool force_check) {
  // check if the modem is ready to answer AT commands
  // We first try to check flags, and then really send an AT command if force_check

  if (!this->dce)
    return false;
  if (!this->internal_state_.modem_synced)
    return false;
  if (!this->cmux_ && this->internal_state_.connected)
    return false;
  if (!this->internal_state_.powered_on)
    return false;
#ifdef USE_MODEM_POWER
  if (this->internal_state_.power_transition)
    return false;
#endif

  if (force_check) {
    if (this->get_imei()) {
      // we are sure that the modem is on
      this->internal_state_.powered_on = true;
      return true;
    } else
      return false;
  } else
    return true;
}

bool ModemComponent::modem_ready() { return this->modem_ready(false); }

void ModemComponent::enable() {
  ESP_LOGD(TAG, "Enabling modem");
  if (this->component_state_ == ModemComponentState::DISABLED) {
    this->component_state_ = ModemComponentState::DISCONNECTED;
  }
  this->internal_state_.start = true;
  this->internal_state_.enabled = true;
}

void ModemComponent::disable() {
  ESP_LOGD(TAG, "Disabling modem");
  this->internal_state_.enabled = false;
  if (this->component_state_ != ModemComponentState::CONNECTED) {
    this->component_state_ = ModemComponentState::DISCONNECTED;
  }
}

network::IPAddresses ModemComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  esp_netif_ip_info_t ip;
  esp_netif_get_ip_info(this->ppp_netif_, &ip);
  addresses[0] = network::IPAddress(&ip.ip);
  return addresses;
}

std::string ModemComponent::get_use_address() const {
  // not usefull for a modem ?
  if (this->use_address_.empty()) {
    return App.get_name() + ".local";
  }
  return this->use_address_;
}

void ModemComponent::setup() {
  ESP_LOGI(TAG, "Setting up Modem...");

  if (this->power_pin_) {
    this->power_pin_->setup();
    // as we have a power pin, we assume that the power is off
    this->internal_state_.powered_on = false;

    if (this->internal_state_.enabled) {
      this->poweron_();
    }
  } else {
    // no status pin, we assume that the power is on
    this->internal_state_.powered_on = true;
  }

  if (this->status_pin_) {
    this->status_pin_->setup();
  }

  ESP_LOGCONFIG(TAG, "Config Modem:");
  ESP_LOGCONFIG(TAG, "  Model     : %s", this->model_.c_str());
  ESP_LOGCONFIG(TAG, "  APN       : %s", this->apn_.c_str());
  ESP_LOGCONFIG(TAG, "  PIN code  : %s", (this->pin_code_.empty()) ? "No" : "Yes (not shown)");
  ESP_LOGCONFIG(TAG, "  Tx Pin    : GPIO%u", this->tx_pin_->get_pin());
  ESP_LOGCONFIG(TAG, "  Rx Pin    : GPIO%u", this->rx_pin_->get_pin());
  ESP_LOGCONFIG(TAG, "  Power pin : %s", (this->power_pin_) ? this->power_pin_->dump_summary().c_str() : "Not defined");
  if (this->status_pin_) {
    std::string current_status = this->get_power_status() ? "ON" : "OFF";
    ESP_LOGCONFIG(TAG, "  Status pin: %s (current state %s)", this->status_pin_->dump_summary().c_str(),
                  current_status.c_str());
  } else {
    ESP_LOGCONFIG(TAG, "  Status pin: Not defined");
  }
  ESP_LOGCONFIG(TAG, "  Enabled   : %s", this->internal_state_.enabled ? "Yes" : "No");
  ESP_LOGCONFIG(TAG, "  Use CMUX  : %s", this->cmux_ ? "Yes" : "No");

  ESP_LOGV(TAG, "PPP netif setup");
  esp_err_t err;
  err = esp_netif_init();
  ESPHL_ERROR_CHECK(err, "PPP netif init error");
  err = esp_event_loop_create_default();
  ESPHL_ERROR_CHECK(err, "PPP event loop init error");

  esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();

  this->ppp_netif_ = esp_netif_new(&netif_ppp_config);
  assert(this->ppp_netif_);

  err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &ModemComponent::ip_event_handler, nullptr,
                                            nullptr);
  ESPHL_ERROR_CHECK(err, "IP event handler register error");

  this->modem_lazy_init_();

  ESP_LOGV(TAG, "Setup finished");
}

void ModemComponent::loop() {
  static ModemComponentState last_state = this->component_state_;
  static uint32_t next_loop_millis = millis();
  static bool connecting = false;
  static uint8_t network_attach_retry = 10;
  static uint8_t ip_lost_retries = 10;

  if ((millis() < next_loop_millis)) {
    // some commands need some delay
    yield();
    return;
  }

  if (this->internal_state_.power_transition) {
    watchdog::WatchdogManager wdt(30000);

    // A power state is used to handle long tonuart/toffuart delay
    switch (this->internal_state_.power_state) {
      case ModemPowerState::TON:
        this->power_pin_->digital_write(false);
        delay(this->power_ton_);
        this->power_pin_->digital_write(true);
        next_loop_millis = millis() + this->power_tonuart_;  // delay for next loop
        this->internal_state_.power_state = ModemPowerState::TONUART;
        ESP_LOGD(TAG, "Will check that the modem is on in %.1fs...", float(this->power_tonuart_) / 1000);
        break;
      case ModemPowerState::TONUART:
        this->internal_state_.power_transition = false;
        ESP_LOGD(TAG, "TONUART check sync");
        if (!this->modem_sync_()) {
          ESP_LOGE(TAG, "Unable to power on the modem");
        } else {
          ESP_LOGI(TAG, "Modem powered ON");
        }
        break;
      case ModemPowerState::TOFF:
        delay(10);
        this->power_pin_->digital_write(false);
        delay(this->power_toff_);
        this->power_pin_->digital_write(true);
        this->internal_state_.power_state = ModemPowerState::TOFFUART;
        ESP_LOGD(TAG, "Will check that the modem is off in %.1fs...", float(this->power_toffuart_) / 1000);
        next_loop_millis = millis() + this->power_toffuart_;  // delay for next loop
        break;
      case ModemPowerState::TOFFUART:
        this->internal_state_.power_transition = false;
        if (this->modem_ready(true)) {
          ESP_LOGE(TAG, "Unable to power off the modem");
          this->internal_state_.powered_on = true;
        } else {
          ESP_LOGI(TAG, "Modem powered OFF");
          this->internal_state_.powered_on = false;
          this->internal_state_.modem_synced = false;
        }
        break;
    }
    App.feed_wdt();
    yield();
    return;
  }

  switch (this->component_state_) {
    case ModemComponentState::NOT_RESPONDING:
      if (this->internal_state_.start) {
        if (this->modem_ready(true)) {
          ESP_LOGI(TAG, "Modem recovered");
          this->status_clear_warning();
          this->component_state_ = ModemComponentState::DISCONNECTED;
        } else {
          this->modem_lazy_init_();
          if (!this->modem_sync_()) {
            ESP_LOGE(TAG, "Unable to recover modem");
          }
          // if (!this->internal_state_.powered_on) {
          //   this->poweron_();
          // }
        }
      }
      break;

    case ModemComponentState::DISCONNECTED:
      if (this->internal_state_.enabled) {
        // be sure the modem is on and synced
        if (!this->internal_state_.powered_on) {
          this->poweron_();
          break;
        } else if (!this->internal_state_.modem_synced) {
          if (!this->modem_sync_()) {
            ESP_LOGE(TAG, "Modem not responding");
            this->component_state_ = ModemComponentState::NOT_RESPONDING;
          }
        }

        if (this->internal_state_.start) {
          // want to connect
          if (!connecting) {
            // wait for the modem be attached to a network, start ppp, and set connecting=true
            if (is_network_attached_()) {
              network_attach_retry = 10;
              if (this->start_ppp_()) {
                connecting = true;
                next_loop_millis = millis() + 2000;  // delay for next loop
              }
            } else {
              ESP_LOGD(TAG, "Waiting for the modem to be attached to a network (left retries: %" PRIu8 ")",
                       network_attach_retry);
              network_attach_retry--;
              if (network_attach_retry == 0) {
                ESP_LOGE(TAG, "modem is unable to attach to a network");
                if (this->power_pin_) {
                  this->poweroff_();
                } else {
                  network_attach_retry = 10;
                  this->component_state_ = ModemComponentState::NOT_RESPONDING;
                }
              }
              next_loop_millis = millis() + 1000;  // delay to retry
            }
          } else {
            // connecting
            if (!this->internal_state_.connected) {
              // wait until this->internal_state_.connected set to true by IP_EVENT_PPP_GOT_IP
              next_loop_millis = millis() + 1000;  // delay for next loop

              // connecting timeout
              if (millis() - this->internal_state_.connect_begin > 15000) {
                ESP_LOGW(TAG, "Connecting via Modem failed! Re-connecting...");
                // TODO: exit data/cmux without error check
                connecting = false;
              }
            } else {
              connecting = false;
              ESP_LOGI(TAG, "Connected via Modem");
              this->component_state_ = ModemComponentState::CONNECTED;

              this->dump_connect_params_();
              this->status_clear_warning();
            }
          }
        } else {
          this->internal_state_.start = true;
        }
      } else {
        this->component_state_ = ModemComponentState::DISABLED;
      }
      break;

    case ModemComponentState::CONNECTED:
      if (this->internal_state_.enabled) {
        if (!this->internal_state_.connected) {
          this->status_set_warning("Connection via Modem lost!");
          this->component_state_ = ModemComponentState::DISCONNECTED;
        }
      } else {
        if (this->internal_state_.connected) {
          // connected but disbaled, so disconnect
          this->stop_ppp_();
          this->component_state_ = ModemComponentState::DISCONNECTED;
        }
      }
      break;

    case ModemComponentState::DISABLED:
      if (this->internal_state_.enabled) {
        this->component_state_ = ModemComponentState::DISCONNECTED;
      } else if (this->internal_state_.powered_on) {
        this->poweroff_();
      }
      next_loop_millis = millis() + 2000;  // delay for next loop
      break;
  }

  if (this->component_state_ != last_state) {
    ESP_LOGV(TAG, "State changed: %s -> %s", state_to_string(last_state).c_str(),
             state_to_string(this->component_state_).c_str());
    this->on_state_callback_.call(last_state, this->component_state_);

    last_state = this->component_state_;
  }
}

void ModemComponent::modem_lazy_init_() {
  // destroy previous dte/dce, and recreate them.
  // no communication is done with the modem.

  this->dte_.reset();
  this->dce.reset();

  esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();

  dte_config.uart_config.tx_io_num = this->tx_pin_->get_pin();
  dte_config.uart_config.rx_io_num = this->rx_pin_->get_pin();
  dte_config.uart_config.rx_buffer_size = this->uart_rx_buffer_size_;
  dte_config.uart_config.tx_buffer_size = this->uart_tx_buffer_size_;
  dte_config.uart_config.event_queue_size = this->uart_event_queue_size_;
  dte_config.task_stack_size = this->uart_event_task_stack_size_;
  dte_config.task_priority = this->uart_event_task_priority_;
  dte_config.dte_buffer_size = this->uart_rx_buffer_size_ / 2;

  this->dte_ = create_uart_dte(&dte_config);

  if (this->dte_->set_mode(modem_mode::COMMAND_MODE)) {
    ESP_LOGD(TAG, "dte in command mode");
  }
  esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(this->apn_.c_str());

  if (this->model_ == "GENERIC") {
    this->dce = create_generic_dce(&dce_config, this->dte_, this->ppp_netif_);
  } else if (this->model_ == "BG96") {
    this->dce = create_BG96_dce(&dce_config, this->dte_, this->ppp_netif_);
  } else if (this->model_ == "SIM800") {
    this->dce = create_SIM800_dce(&dce_config, this->dte_, this->ppp_netif_);
  } else if (this->model_ == "SIM7000") {
    this->dce = create_SIM7000_dce(&dce_config, this->dte_, this->ppp_netif_);
  } else if (this->model_ == "SIM7600" || this->model_ == "SIM7670") {
    this->dce = create_SIM7600_dce(&dce_config, this->dte_, this->ppp_netif_);
  } else {
    ESP_LOGE(TAG, "Invalid model %s", this->model_.c_str());
    return;
  }

  // flow control not fully implemented, but kept here for future work
  // if (dte_config.uart_config.flow_control == ESP_MODEM_FLOW_CONTROL_HW) {
  //   if (command_result::OK != this->dce->set_flow_control(2, 2)) {
  //     ESP_LOGE(TAG, "Failed to set the set_flow_control mode");
  //     return;
  //   }
  //   ESP_LOGD(TAG, "set_flow_control OK");
  // }

  ESP_LOGV(TAG, "DTE and CDE created");
}

bool ModemComponent::modem_sync_() {
  // force command mode, check sim, and send init_at commands
  // close cmux/data if needed, and may reboot the modem.

  uint32_t start_ms = millis();
  uint32_t elapsed_ms;
  std::string result;

  ESP_LOGV(TAG, "Checking if the modem is synced...");
  bool status = this->get_imei();
  if (!status) {
    // Try to exit CMUX_MANUAL_DATA or DATA_MODE, if any
    ESP_LOGD(TAG, "Connecting to the the modem...");
    watchdog::WatchdogManager wdt(30000);

    auto command_mode = [this]() -> bool {
      ESP_LOGVV(TAG, "trying command mode");
      this->dce->set_mode(modem_mode::UNDEF);
      return this->dce->set_mode(modem_mode::COMMAND_MODE) && this->get_imei();
    };

    auto cmux_command_mode = [this]() -> bool {
      ESP_LOGVV(TAG, "trying cmux command mode");
      return this->dce->set_mode(modem_mode::CMUX_MANUAL_MODE) &&
             this->dce->set_mode(modem_mode::CMUX_MANUAL_COMMAND) && this->get_imei();
    };

    // The cmux state is supposed to be the same before the reboot. But if it has changed (new firwmare), we will try
    // to fallback to inverted cmux state.
    if (this->cmux_) {
      status = cmux_command_mode() || (command_mode() && cmux_command_mode());
    } else {
      status = command_mode() || (cmux_command_mode() && command_mode());
    }

    elapsed_ms = millis() - start_ms;

    if (!status) {
      ESP_LOGW(TAG, "modem not responding after %" PRIu32 "ms.", elapsed_ms);
      // assume the modem is OFF
      this->internal_state_.powered_on = false;
    } else {
      ESP_LOGD(TAG, "Connected to the modem in %" PRIu32 "ms", elapsed_ms);
      this->internal_state_.powered_on = true;
    }
  } else {
    // modem responded without need to recover command mode
    this->internal_state_.powered_on = true;
  }

  if (status && !this->internal_state_.modem_synced) {
    // First time the modem is synced, or modem recovered
    App.feed_wdt();
    watchdog::WatchdogManager wdt(30000);
    this->internal_state_.modem_synced = true;

    if (!this->prepare_sim_()) {
      // fatal error
      this->disable();
      status = false;
    }
    this->send_init_at_();

    ESP_LOGI(TAG, "Modem infos:");
    std::string result;
    ESPMODEM_ERROR_CHECK(this->dce->get_module_name(result), "get_module_name");
    ESP_LOGI(TAG, "  Module name: %s", result.c_str());
  }

  this->internal_state_.modem_synced = status;

  ESP_LOGVV(TAG, "Sync end status: %d", this->internal_state_.modem_synced);

  return status;
}

bool ModemComponent::prepare_sim_() {
  // it seems that read_pin(pin_ok) unexpectedly fail if no sim card is inserted, whithout updating the 'pin_ok'
  bool pin_ok = false;
  if (this->dce->read_pin(pin_ok) != command_result::OK) {
    this->status_set_error("Unable to read pin status. Missing SIM card?");
    return false;
  }

  if (!pin_ok) {
    if (!this->pin_code_.empty()) {
      ESP_LOGV(TAG, "Set pin code: %s", this->pin_code_.c_str());
      ESPMODEM_ERROR_CHECK(this->dce->set_pin(this->pin_code_), "Set pin code failed");
      delay(this->command_delay_);
    }
  }

  this->dce->read_pin(pin_ok);
  if (pin_ok) {
    if (this->pin_code_.empty()) {
      ESP_LOGD(TAG, "PIN not needed");
    } else {
      ESP_LOGD(TAG, "PIN unlocked");
    }
  } else {
    this->status_set_error("Invalid PIN code.");
  }
  return pin_ok;
}

void ModemComponent::send_init_at_() {
  // send initial AT commands from yaml
  for (const auto &cmd : this->init_at_commands_) {
    App.feed_wdt();
    auto at_command_result = this->send_at(cmd);
    if (!at_command_result) {
      ESP_LOGE(TAG, "Error while executing 'init_at' '%s' command", cmd.c_str());
    } else {
      ESP_LOGI(TAG, "'init_at' '%s' result: %s", cmd.c_str(), at_command_result.result.c_str());
    }
    delay(200);  // NOLINT
  }
}

bool ModemComponent::is_network_attached_() {
  if (this->internal_state_.connected)
    return true;
  if (this->modem_ready()) {
    int attached = 99;
    this->dce->get_network_attachment_state(attached);
    if (attached != 99)
      return (bool) attached;
  }
  return false;
}

bool ModemComponent::start_ppp_() {
  this->internal_state_.connect_begin = millis();
  this->status_set_warning("Starting connection");
  watchdog::WatchdogManager wdt(10000);

  // will be set to true on event IP_EVENT_PPP_GOT_IP
  this->internal_state_.got_ipv4_address = false;

  ESP_LOGD(TAG, "Asking the modem to enter PPP");

  bool status = false;

  if (cmux_) {
    this->dce->set_mode(modem_mode::CMUX_MANUAL_MODE);
    status = this->dce->set_mode(modem_mode::CMUX_MANUAL_DATA) && this->modem_ready();
  } else {
    status = this->dce->set_mode(modem_mode::DATA_MODE);
  }

  if (!status) {
    ESP_LOGE(TAG, "Unable to change modem mode to PPP");
  }

  return status;
}

bool ModemComponent::stop_ppp_() {
  bool status = false;
  watchdog::WatchdogManager wdt(10000);
  if (this->cmux_) {
    status = this->dce->set_mode(modem_mode::CMUX_MANUAL_COMMAND);
  } else {
    status = this->dce->set_mode(modem_mode::COMMAND_MODE);
  }
  if (!status) {
    ESP_LOGE(TAG, "Error exiting PPP");
  }
  return status;
}

void ModemComponent::ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  ip_event_got_ip_t *event;
  const esp_netif_ip_info_t *ip_info;
  switch (event_id) {
    case IP_EVENT_PPP_GOT_IP:
      event = (ip_event_got_ip_t *) event_data;
      ip_info = &event->ip_info;
      ESP_LOGD(TAG, "[IP event] Got IP " IPSTR, IP2STR(&ip_info->ip));
      global_modem_component->internal_state_.got_ipv4_address = true;
      global_modem_component->internal_state_.connected = true;
      break;

    case IP_EVENT_PPP_LOST_IP:
      if (global_modem_component->internal_state_.connected) {
        // do not log message if we are not connected
        ESP_LOGD(TAG, "[IP event] Lost IP");
      }
      global_modem_component->internal_state_.got_ipv4_address = false;
      global_modem_component->internal_state_.connected = false;
      break;
  }
}

void ModemComponent::poweron_() {
  if (this->power_pin_) {
    this->internal_state_.power_state = ModemPowerState::TON;
    this->internal_state_.power_transition = true;
  } else {
    if (this->modem_ready()) {
      ESP_LOGV(TAG, "Modem is already ON");
    } else {
      ESP_LOGE(TAG, "No 'power_pin' defined: Not able to poweron the modem");
    }
  }
}

void ModemComponent::poweroff_() {
  if (this->power_pin_) {
    this->internal_state_.power_state = ModemPowerState::TOFF;
    this->internal_state_.power_transition = true;
  }
}

void ModemComponent::dump_connect_params_() {
  if (!this->internal_state_.connected) {
    ESP_LOGCONFIG(TAG, "Modem connection: Not connected");
    return;
  }
  esp_netif_ip_info_t ip;
  esp_netif_get_ip_info(this->ppp_netif_, &ip);
  ESP_LOGCONFIG(TAG, "Modem connection:");
  ESP_LOGCONFIG(TAG, "  IP Address  : %s", network::IPAddress(&ip.ip).str().c_str());
  ESP_LOGCONFIG(TAG, "  Hostname    : '%s'", App.get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Subnet      : %s", network::IPAddress(&ip.netmask).str().c_str());
  ESP_LOGCONFIG(TAG, "  Gateway     : %s", network::IPAddress(&ip.gw).str().c_str());

  const ip_addr_t *dns_main_ip = dns_getserver(ESP_NETIF_DNS_MAIN);
  const ip_addr_t *dns_backup_ip = dns_getserver(ESP_NETIF_DNS_BACKUP);
  const ip_addr_t *dns_fallback_ip = dns_getserver(ESP_NETIF_DNS_FALLBACK);

  ESP_LOGCONFIG(TAG, "  DNS main    : %s", network::IPAddress(dns_main_ip).str().c_str());
  ESP_LOGCONFIG(TAG, "  DNS backup  : %s", network::IPAddress(dns_backup_ip).str().c_str());
  ESP_LOGCONFIG(TAG, "  DNS fallback: %s", network::IPAddress(dns_fallback_ip).str().c_str());
}

}  // namespace modem
}  // namespace esphome

#endif
