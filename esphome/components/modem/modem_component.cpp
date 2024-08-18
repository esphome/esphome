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

void ModemComponent::enable_debug() { esp_log_level_set("command_lib", ESP_LOG_VERBOSE); }

bool ModemComponent::is_modem_connected(bool verbose) {
  float rssi, ber;
  int network_mode = 0;
  bool network_attached = this->is_network_attached_();
  this->get_signal_quality(rssi, ber);
  this->dce->get_network_system_mode(network_mode);

  bool connected = (network_mode != 0) && (!std::isnan(rssi)) && network_attached;

  ESP_LOGI(TAG, "Modem internal network status: %s (attached: %s, type: %s, rssi: %.0fdB %s, ber: %.0f%%)",
           connected ? "Good" : "BAD", network_attached ? "Yes" : "NO",
           network_system_mode_to_string(network_mode).c_str(), rssi, get_signal_bars(rssi).c_str(), ber);
  return connected;
}

AtCommandResult ModemComponent::send_at(const std::string &cmd, uint32_t timeout) {
  AtCommandResult at_command_result;
  at_command_result.success = false;
  at_command_result.esp_modem_command_result = command_result::TIMEOUT;
  if (this->dce) {
    at_command_result.esp_modem_command_result = this->dce->at(cmd, at_command_result.output, timeout);
    ESP_LOGV(TAG, "Result for command %s: %s (status %s)", cmd.c_str(), at_command_result.c_str(),
             command_result_to_string(at_command_result.esp_modem_command_result).c_str());
  }
  at_command_result.success = at_command_result.esp_modem_command_result == command_result::OK;
  return at_command_result;
}

AtCommandResult ModemComponent::get_imei() {
  // get the imei, and check the result is a valid imei string
  // (so it can be used to check if the modem is responding correctly (a simple 'AT' cmd is sometime not enough))
  AtCommandResult at_command_result = this->send_at("AT+CGSN", this->command_delay_);
  if (at_command_result.success && at_command_result.output.length() == 15) {
    for (char c : at_command_result.output) {
      if (!isdigit(static_cast<unsigned char>(c))) {
        at_command_result.success = false;
        break;
      }
    }
  } else {
    at_command_result.success = false;
  }
  ESP_LOGV(TAG, "imei: %s (status: %s)", at_command_result.c_str(),
           command_result_to_string(at_command_result.esp_modem_command_result).c_str());
  return at_command_result;
}

int ModemComponent::get_baud_rate_() {
  AtCommandResult at_command_result = this->send_at("AT+IPR?");
  const std::string sep = ": ";
  size_t pos = at_command_result.output.find(sep);
  if (pos == std::string::npos) {
    ESP_LOGE(TAG, "Unable to get baud rate");
    return -1;
  } else {
    return (std::stoi(at_command_result.output.substr(pos + sep.length())));
  }
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

bool ModemComponent::sync() {
  this->internal_state_.modem_synced = this->dce->sync() == command_result::OK;
  if (this->internal_state_.modem_synced) {
    this->internal_state_.powered_on = true;
    this->modem_restore_state_.synced = true;
  }
  return this->internal_state_.modem_synced;
  // extensive check with imei
  //  this->internal_state_.modem_synced = this->get_imei();
  //  if (this->internal_state_.modem_synced) {
  //    this->internal_state_.powered_on = true;
  //    this->modem_restore_state_.synced = true;
  //  }
  //  return this->internal_state_.modem_synced;
}

bool ModemComponent::modem_ready(bool force_check) {
  // check if the modem is ready to answer AT commands
  // We first try to check flags, and then really send an AT command if force_check

  if (!this->internal_state_.modem_synced)
    return false;
  if (!this->cmux_ && this->internal_state_.connected)
    return false;
  if (!this->internal_state_.powered_on)
    return false;
  if (this->internal_state_.power_transition)
    return false;

  if (force_check) {
    if (this->sync()) {
      // we are sure that the modem is on
      this->internal_state_.powered_on = true;
      return true;
    } else
      return false;
  } else
    return true;
}

void ModemComponent::enable() {
  ESP_LOGD(TAG, "Enabling modem");
  if (this->component_state_ == ModemComponentState::DISABLED) {
    this->component_state_ = ModemComponentState::DISCONNECTED;
  }
  this->internal_state_.enabled = true;
}

void ModemComponent::disable() {
  ESP_LOGD(TAG, "Disabling modem");
  this->internal_state_.enabled = false;
  this->internal_state_.starting = false;
  if (this->component_state_ != ModemComponentState::CONNECTED) {
    this->component_state_ = ModemComponentState::DISCONNECTED;
  }
}

void ModemComponent::reconnect() {
  if (!this->internal_state_.reconnect) {
    this->internal_state_.reconnect = true;
    this->internal_state_.connected = false;
    this->component_state_ = ModemComponentState::DISCONNECTED;
    // if reconnect fail, let some time before retry
    set_timeout("retry_reconnect", this->reconnect_grace_period_,
                [this]() { this->internal_state_.reconnect = false; });
  } else {
    ESP_LOGD(TAG, "Reconnecting already in progress.");
  }
}

bool ModemComponent::get_signal_quality(float &rssi, float &ber) {
  rssi = NAN;
  ber = NAN;
  int modem_rssi = 99;
  int modem_ber = 99;
  if (this->modem_ready() &&
      (global_modem_component->dce->get_signal_quality(modem_rssi, modem_ber) == command_result::OK)) {
    if (modem_rssi != 99)
      rssi = -113 + (modem_rssi * 2);
    if (modem_ber != 99)
      ber = 0.1f * (modem_ber * modem_ber);
    return true;
  }
  return false;
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
  this->pref_ = global_preferences->make_preference<ModemRestoreState>(76007670UL);
  this->pref_.load(&this->modem_restore_state_);

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

  // create dte/dce with default baud rate (we assume a cold modem start)
  this->modem_create_dce_dte_();  // real init will be done by enable

  ESP_LOGV(TAG, "Setup finished");
}

void ModemComponent::loop() {
  static ModemComponentState last_state = this->component_state_;
  static uint32_t next_loop_millis = millis();
  static uint32_t last_health_check = millis();
  static bool connecting = false;

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
        ESP_LOGD(TAG, "TONUART check sync");
        if (!this->modem_init_()) {
          ESP_LOGE(TAG, "Unable to power on the modem");
        } else {
          ESP_LOGI(TAG, "Modem powered ON");
        }
        this->internal_state_.power_transition = false;
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
      if (this->internal_state_.starting) {
        ESP_LOGW(TAG, "Modem not responding, resetting...");
        this->internal_state_.connected = false;
        // this->modem_create_dce_dte_();
        if (!this->modem_init_()) {
          ESP_LOGE(TAG, "Unable to recover modem");
        } else {
          this->component_state_ = ModemComponentState::DISCONNECTED;
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
          if (!this->modem_init_()) {
            this->component_state_ = ModemComponentState::NOT_RESPONDING;
          }
        }

        if (this->internal_state_.starting) {
          float time_left_s = float(this->timeout_ - (millis() - this->internal_state_.startms)) / 1000;
          // want to connect
          if ((millis() - this->internal_state_.startms) > this->timeout_) {
            this->abort_("Timeout while trying to connect");
          }
          if (!connecting) {
            // wait for the modem be attached to a network, start ppp, and set connecting=true
            if (this->is_modem_connected()) {
              if (this->start_ppp_()) {
                connecting = true;
              } else {
                ESP_LOGE(TAG, "modem is unable to enter PPP (time left before abort: %.0fs)", time_left_s);
                this->stop_ppp_();
                this->is_modem_connected();
              }
            } else {
              ESP_LOGW(TAG, "Waiting for the modem to be attached to a network (time left before abort: %.0fs)",
                       time_left_s);
              next_loop_millis = millis() + 5000;  // delay to retry
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
          this->internal_state_.starting = true;
          this->internal_state_.startms = millis();
        }
      } else {
        this->component_state_ = ModemComponentState::DISABLED;
      }
      break;

    case ModemComponentState::CONNECTED:
      this->internal_state_.starting = false;
      if (this->internal_state_.enabled) {
        if (!this->internal_state_.connected) {
          this->status_set_warning("Connection via Modem lost!");
          this->component_state_ = ModemComponentState::DISCONNECTED;
          break;
        }

        if (this->cmux_ && (millis() - last_health_check) > 30000) {
          last_health_check = millis();
          if (!this->is_modem_connected()) {
            ESP_LOGW(TAG, "Reconnecting...");
            this->reconnect();
          }
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

void ModemComponent::modem_create_dce_dte_(int baud_rate) {
  // create or recreate dte and dce.
  // no communication is done with the modem.

  this->internal_state_.modem_synced = false;

  esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();

  dte_config.uart_config.tx_io_num = this->tx_pin_->get_pin();
  dte_config.uart_config.rx_io_num = this->rx_pin_->get_pin();
  dte_config.uart_config.rx_buffer_size = this->uart_rx_buffer_size_;
  dte_config.uart_config.tx_buffer_size = this->uart_tx_buffer_size_;
  dte_config.uart_config.event_queue_size = this->uart_event_queue_size_;
  if (baud_rate != 0) {
    ESP_LOGD(TAG, "DTE config baud rate: %d", baud_rate);
    dte_config.uart_config.baud_rate = baud_rate;
  }
  dte_config.task_stack_size = this->uart_event_task_stack_size_;
  dte_config.task_priority = this->uart_event_task_priority_;
  dte_config.dte_buffer_size = this->uart_rx_buffer_size_ / 2;

  this->dce.reset();
  this->dte_.reset();
  this->dte_ = create_uart_dte(&dte_config);

  if (!this->dte_->set_mode(modem_mode::COMMAND_MODE)) {
    ESP_LOGW(TAG, "Unable to set DTE in command mode.");
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

  ESP_LOGV(TAG, "DTE and DCE created");
}

bool ModemComponent::modem_command_mode_(bool cmux) {
  bool success;
  if (cmux) {
    success = this->dce->set_mode(modem_mode::CMUX_MANUAL_MODE) &&
              this->dce->set_mode(modem_mode::CMUX_MANUAL_COMMAND) && this->sync();
  } else {
    this->dce->set_mode(modem_mode::UNDEF);
    success = this->dce->set_mode(modem_mode::COMMAND_MODE) && this->sync();
  }
  ESP_LOGV(TAG, "command mode using %s: %s", cmux ? "CMUX" : "Single channel", success ? "Success" : "Error");
  return success;
}

bool ModemComponent::modem_recover_sync_(int baud_rate) {
  // the modem is not responding. possible causes are:
  //  - warm reboot, it's still in data or cmux mode.
  //  - has a non default baud rate
  //  - power off

  ESP_LOGD(TAG, "Brute force recovering command mode with baud rate %d", baud_rate);
  this->modem_create_dce_dte_(baud_rate);

  bool success = false;
  this->modem_restore_state_.synced = false;

  // Try to exit CMUX_MANUAL_DATA or DATA_MODE, if any
  // huge watchdog, because some commands are blocking for a very long time.
  watchdog::WatchdogManager wdt(60000);

  // The cmux state is supposed to be the same before the reboot. But if it has changed (new firwmare), we will try
  // to fallback to inverted cmux state.
  success = this->modem_command_mode_(this->cmux_) ||
            (this->modem_command_mode_(!this->cmux_) && this->modem_command_mode_(this->cmux_));

  ESP_LOGD(TAG, "Brute force recover state: %s", success ? "OK" : "NOK");

  return success;
}

bool ModemComponent::modem_preinit_() {
  // init the modem to get command mode.
  // if baud_rate != 0, will also set the baud rate.

  // std::string result;
  uint32_t start_ms = millis();
  uint32_t elapsed_ms;

  ESP_LOGV(TAG, "Checking if the modem is reachable...");

  bool success = false;
  if (this->sync()) {
    // should be reached if modem cold start (default baud rate)
    ESP_LOGD(TAG, "Modem responded at 1st attempt");
    success = true;
  }

  if (!success) {
    // we assume a warm modem restart, so we restore baud rate
    this->modem_create_dce_dte_(this->modem_restore_state_.baud_rate);
    if (this->sync()) {
      ESP_LOGD(TAG, "Modem responded after restoring baud rate %d", this->modem_restore_state_.baud_rate);
      success = true;
    }
  }

  if (!success) {
    watchdog::WatchdogManager wdt(60000);
    // this->dce->set_mode(modem_mode::CMUX_MANUAL_MODE);
    // this->dce->set_mode(modem_mode::CMUX_MANUAL_DATA);
    // this->dce->recover();
    this->dce->set_mode(modem_mode::UNDEF);
    if (this->modem_command_mode_(this->modem_restore_state_.cmux)) {
      ESP_LOGD(TAG, "Modem responded after recovering command mode");
      success = true;
    }
  }

  if (!success) {
    // brute force recover
    success = this->modem_recover_sync_(this->modem_restore_state_.baud_rate) || this->modem_recover_sync_() ||
              this->modem_recover_sync_(this->baud_rate_);
  }

  if (!success) {
    ESP_LOGE(TAG, "Fatal: modem not responding during init");
    return false;
  } else {
    ESP_LOGD(TAG, "Communication with the modem established");
  }

  this->modem_restore_state_.cmux = this->cmux_;

  int current_baud_rate = this->get_baud_rate_();
  ESP_LOGD(TAG, "current baud rate: %d", current_baud_rate);
  this->modem_restore_state_.baud_rate = current_baud_rate;
  this->pref_.save(&this->modem_restore_state_);

  // modem synced
  if ((this->baud_rate_ != 0) && (this->baud_rate_ != current_baud_rate)) {
    ESP_LOGD(TAG, "Setting baud rate: %d", this->baud_rate_);
    this->flush_uart_();
    // if (this->dce->set_baud(this->baud_rate_) == command_result::OK) {
    // no error check, because the modem answer with a different baud rate
    this->dce->set_baud(this->baud_rate_);
    // need to recreate dte/dce with new baud rate
    this->modem_create_dce_dte_(this->baud_rate_);
    delay(1000);  // NOLINT
    if (this->sync()) {
      ESP_LOGI(TAG, "Modem baud rate set to %d", this->baud_rate_);
      success = true;
      this->modem_restore_state_.baud_rate = this->baud_rate_;
      this->pref_.save(&this->modem_restore_state_);
    } else {
      // revert baud rate: FIXME: or wait safe mode ?
      this->modem_create_dce_dte_();
      delay(200);  // NOLINT
      this->flush_uart_();
      if (this->sync()) {
        ESP_LOGW(TAG, "Unable to change baud rate, keeping default");
      } else {
        this->abort_("DCE has successfuly changed baud rate, but DTE can't reach it. Try to decrease baud rate?");
        return false;
      }
    }
  }

  elapsed_ms = millis() - start_ms;

  if (success) {
    ESP_LOGI(TAG, "Modem initialized in %" PRIu32 "ms", elapsed_ms);
  } else {
    ESP_LOGE(TAG, "Unable to initialize modem in %" PRIu32 "ms", elapsed_ms);
  }

  return success;
}

bool ModemComponent::modem_init_() {
  // force command mode, check sim, and send init_at commands
  // close cmux/data if needed, and may reboot the modem.

  bool success = this->modem_preinit_();

  if (!success) {
    ESP_LOGE(TAG, "Fatal: modem not responding");
    return false;
  }

  this->pref_.save(&this->modem_restore_state_);
  global_preferences->sync();

  ESP_LOGI(TAG, "Modem infos:");
  std::string result;
  ESPMODEM_ERROR_CHECK(this->dce->get_module_name(result), "get_module_name");
  ESP_LOGI(TAG, "  Module name: %s", result.c_str());

  this->send_init_at_();

  if (!this->prepare_sim_()) {
    this->abort_("Fatal: Sim error");
    return false;
  }

  success = this->sync();

  if (!success) {
    ESP_LOGE(TAG, "Fatal: unable to init modem");
  }

  return success;
}

bool ModemComponent::prepare_sim_() {
  std::string output;
  this->flush_uart_();
  // this->dce->read_pin(pin_ok)   // not used, because we can't know the cause of the error.
  this->dce->command(
      "AT+CPIN?\r",
      [&](uint8_t *data, size_t len) {
        output.assign(reinterpret_cast<char *>(data), len);
        std::replace(output.begin(), output.end(), '\n', ' ');
        return command_result::OK;
      },
      this->command_delay_);

  ESP_LOGD(TAG, "SIM: %s", output.c_str());

  if ((output.find("+CPIN: READY") != std::string::npos) || (output.find("+CPIN: SIM PIN") != std::string::npos)) {
    return true;  // pin not needed or already unlocked
  } else {
    if (output.find("SIM not inserted") != std::string::npos) {
      return false;
    }
  }

  ESPMODEM_ERROR_CHECK(this->dce->set_pin(this->pin_code_), "Set pin error");

  bool pin_ok = false;
  ESPMODEM_ERROR_CHECK(this->dce->read_pin(pin_ok), "Error checking pin");

  return pin_ok;
}

void ModemComponent::send_init_at_() {
  // send initial AT commands from yaml
  for (const auto &cmd : this->init_at_commands_) {
    App.feed_wdt();

    std::string output;

    ESPMODEM_ERROR_CHECK(this->dce->command(
                             cmd + "\r",
                             [&](uint8_t *data, size_t len) {
                               output.assign(reinterpret_cast<char *>(data), len);
                               std::replace(output.begin(), output.end(), '\n', ' ');
                               return command_result::OK;
                             },
                             this->command_delay_),
                         "init_at");
    delay(200);                         // NOLINT
    output += this->flush_uart_(2000);  // probably a bug in esp_modem. long string are truncated
    ESP_LOGI(TAG, "init_at %s: %s", cmd.c_str(), output.c_str());
  }
  this->flush_uart_();
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
  watchdog::WatchdogManager wdt(15000);  // mini 10000

  uint32_t now = millis();

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
    ESP_LOGE(TAG, "Unable to change modem mode to PPP after %" PRIu32 "ms", millis() - now);
  } else {
    ESP_LOGD(TAG, "Entered PPP after %" PRIu32 "ms", millis() - now);
  }
  this->pref_.save(&this->modem_restore_state_);
  return status;
}

bool ModemComponent::stop_ppp_() {
  watchdog::WatchdogManager wdt(10000);
  bool status = this->modem_command_mode_();
  if (!status) {
    ESP_LOGW(TAG, "Error exiting PPP");
  }
  this->pref_.save(&this->modem_restore_state_);
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
  } else {
    // This will powercycle the modem
    this->send_at("AT+CPOF");
  }
}

void ModemComponent::abort_(const std::string &message) {
  ESP_LOGE(TAG, "Aborting: %s", message.c_str());
  this->modem_restore_state_.abort_count++;
  this->pref_.save(&this->modem_restore_state_);
  App.reboot();
}

void ModemComponent::dump_connect_params_() {
  if (!this->internal_state_.connected) {
    ESP_LOGCONFIG(TAG, "Modem connection: Not connected");
    return;
  }
  esp_netif_ip_info_t ip;
  esp_netif_get_ip_info(this->ppp_netif_, &ip);
  ESP_LOGI(TAG, "Modem connection:");
  ESP_LOGI(TAG, "  IP Address  : %s", network::IPAddress(&ip.ip).str().c_str());
  ESP_LOGI(TAG, "  Hostname    : '%s'", App.get_name().c_str());
  ESP_LOGI(TAG, "  Subnet      : %s", network::IPAddress(&ip.netmask).str().c_str());
  ESP_LOGI(TAG, "  Gateway     : %s", network::IPAddress(&ip.gw).str().c_str());

  const ip_addr_t *dns_main_ip = dns_getserver(ESP_NETIF_DNS_MAIN);
  const ip_addr_t *dns_backup_ip = dns_getserver(ESP_NETIF_DNS_BACKUP);
  const ip_addr_t *dns_fallback_ip = dns_getserver(ESP_NETIF_DNS_FALLBACK);

  ESP_LOGCONFIG(TAG, "  DNS main    : %s", network::IPAddress(dns_main_ip).str().c_str());
  ESP_LOGCONFIG(TAG, "  DNS backup  : %s", network::IPAddress(dns_backup_ip).str().c_str());
  ESP_LOGCONFIG(TAG, "  DNS fallback: %s", network::IPAddress(dns_fallback_ip).str().c_str());
}

std::string ModemComponent::flush_uart_(uint32_t timeout) {
  // empty command that just read the result, to flush the uart
  size_t cleaned = 0;
  std::string output;
  this->dce->command(
      "",
      [&](uint8_t *data, size_t len) {
        cleaned = len;
        output.assign(reinterpret_cast<char *>(data), len);
        return command_result::OK;
      },
      timeout);

  if (cleaned != 0) {
    ESP_LOGV(TAG, "Flushed %d modem buffer data: %s", cleaned, output.c_str());
  }
  return output;
}

const char *AtCommandResult::c_str() const {
  if (success) {
    cached_c_str = output + " (OK)";
  } else {
    cached_c_str = output + " (" + command_result_to_string(esp_modem_command_result) + ")";
  }
  return cached_c_str.c_str();
}

}  // namespace modem
}  // namespace esphome

#endif
