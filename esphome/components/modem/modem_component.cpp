#ifdef USE_ESP_IDF
#include "modem_component.h"
#include "modem_handler.h"
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

static const char *const TAG = "modem";

ModemComponent *global_modem_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

ModemComponent::ModemComponent() {
  assert(global_modem_component == nullptr);
  global_modem_component = this;
  this->modem_handler = std::make_unique<ModemHandler>();
}

// Delegated methods
AtCommandResult ModemComponent::send_at(const std::string &cmd, uint32_t timeout, bool verbose) {
  return this->modem_handler->send_at(cmd, timeout, verbose);
}

void ModemComponent::enable() {
  if (this->component_state_ == ModemComponentState::DISABLED) {
    ESP_LOGI(TAG, "Enabling modem");
    set_timeout("modem timeout", this->timeout_, [this]() { this->abort_("Modem was not able to connect (timeout)"); });
    // this->enable_loop();
    this->component_state_ = ModemComponentState::ENABLING;
  }
}

void ModemComponent::disable() {
  this->disable_wanted_ = true;
  this->component_state_ = ModemComponentState::DISABLING;
}

void ModemComponent::reset() {
  this->component_state_ = ModemComponentState::DISABLING;
  this->disable_wanted_ = false;
}

network::IPAddresses ModemComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  if (this->component_state_ == ModemComponentState::CONNECTED) {
    addresses[0] = network::IPAddress(&this->modem_handler->network_infos.ip_info.ip);
  }
  return addresses;
}

const char *ModemComponent::get_use_address() const {
  // Not useful for a modem?
  if (this->use_address_.empty()) {
    static std::string full_name;
    full_name = App.get_name() + ".local";
    return full_name.c_str();
  }
  return this->use_address_.c_str();
}

void ModemComponent::setup() {
  ESP_LOGI(TAG, "Modem setup...State: %s", state_to_string(this->component_state_).c_str());
  this->pref_ = global_preferences->make_preference<ModemRestoreState>(76007670UL);
  this->pref_.load(&this->modem_restore_state_);

  if (this->modem_handler->power_pin) {
    this->modem_handler->power_pin->setup();
    this->modem_handler->power_pin->digital_write(true);
  }
  if (this->modem_handler->status_pin) {
    this->modem_handler->status_pin->setup();
    this->modem_handler->status_pin->pin_mode(gpio::Flags::FLAG_INPUT | gpio::Flags::FLAG_PULLUP);
  }

  // find an available UART
  for (auto p : {UART_NUM_2, UART_NUM_1}) {
    if (!uart_is_driver_installed(p)) {
      this->modem_handler->uart_port_num = p;
      break;
    }
  }
  if (this->modem_handler->uart_port_num == UART_NUM_MAX) {
    ESP_LOGE(TAG, "No free UART port for modem");
    this->mark_failed("No free UART port for modem");
    return;
  }

  ESP_LOGCONFIG(TAG, "Config Modem:");
  ESP_LOGCONFIG(TAG, "  Model     : %s", this->modem_handler->model.c_str());
  ESP_LOGCONFIG(TAG, "  APN       : %s", this->modem_handler->apn.c_str());
  ESP_LOGCONFIG(TAG, "  PIN code  : %s", (this->modem_handler->pin_code.empty()) ? "No" : "Yes (not shown)");
  ESP_LOGCONFIG(TAG, "  Tx Pin    : GPIO%u", this->modem_handler->tx_pin->get_pin());
  ESP_LOGCONFIG(TAG, "  Rx Pin    : GPIO%u", this->modem_handler->rx_pin->get_pin());
  ESP_LOGCONFIG(
      TAG, "  Power pin : %s",
      (this->modem_handler->power_pin) ? this->modem_handler->power_pin->dump_summary().c_str() : "Not defined");
  if (this->modem_handler->power_pin) {
    ESP_LOGCONFIG(TAG, "    ON pulse delay  : %dms", this->modem_handler->power_ton_pulse_delay);
    ESP_LOGCONFIG(TAG, "    ON delay        : %dms", this->modem_handler->power_ton_delay);
    ESP_LOGCONFIG(TAG, "    OFF pulse delay : %dms", this->modem_handler->power_toff_pulse_delay);
    ESP_LOGCONFIG(TAG, "    OFF delay       : %dms", this->modem_handler->power_toff_delay);
  }
  if (this->modem_handler->status_pin) {
    std::string current_status = this->modem_handler->get_power_status() ? "ON" : "OFF";
    ESP_LOGCONFIG(TAG, "  Status pin: %s (state: %s)", this->modem_handler->status_pin->dump_summary().c_str(),
                  current_status.c_str());
  } else {
    ESP_LOGCONFIG(TAG, "  Status pin: None");
  }
  ESP_LOGCONFIG(TAG, "  Enabled   : %s", (this->component_state_ != ModemComponentState::DISABLED) ? "Yes" : "No");
  ESP_LOGCONFIG(TAG, "  Use CMUX  : %s", this->modem_handler->cmux ? "Yes" : "No");
  if (this->modem_handler->baud_rate != 0)
    ESP_LOGCONFIG(TAG, "  Baud rate : %d", this->modem_handler->baud_rate);
  ESP_LOGCONFIG(TAG, "  UART port : %d", this->modem_handler->uart_port_num);
  ESP_LOGCONFIG(TAG, "  TX  buffer size     : %d", this->modem_handler->tx_buffer_size);
  ESP_LOGCONFIG(TAG, "  RX  buffer size     : %d", this->modem_handler->rx_buffer_size);
  ESP_LOGCONFIG(TAG, "  DTE buffer size     : %d", this->modem_handler->dte_buffer_size);

  if (CONFIG_ESP_TASK_WDT_TIMEOUT_S <= 10) {
    ESP_LOGW(TAG, "WDT timeout (%d s) may be too low for modem. Increase if WDT triggers.",
             CONFIG_ESP_TASK_WDT_TIMEOUT_S);
  }

  ESP_LOGV(TAG, "PPP netif init.");
  esp_err_t err;
  err = esp_netif_init();
  ESPHL_ERROR_CHECK(err, "PPP netif init failed");
  err = esp_event_loop_create_default();
  ESPHL_ERROR_CHECK(err, "PPP event loop init failed");

  esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
  this->modem_handler->ppp_netif = esp_netif_new(&netif_ppp_config);
  assert(this->modem_handler->ppp_netif);

  err = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ModemHandler::ip_event_handler,
                                   this->modem_handler.get());
  ESPHL_ERROR_CHECK(err, "IP event handler register failed");

  this->modem_handler->modem_create_dte_dce(this->modem_handler->baud_rate);

  ESP_LOGV(TAG, "Setup complete. State: %s", state_to_string(this->component_state_).c_str());
}

void ModemComponent::loop() {
  if ((millis() < this->next_loop_millis_)) {
    // Some commands require a delay.
    delay(10);
    return;
  }
  if (this->component_state_ != this->component_last_state_) {
    ESP_LOGV(TAG, "State change: %s -> %s", state_to_string(this->component_last_state_).c_str(),
             state_to_string(this->component_state_).c_str());
    this->on_state_callback_.call(this->component_last_state_, this->component_state_);

    this->component_last_state_ = this->component_state_;
  }

  switch (this->component_state_) {
    case ModemComponentState::ENABLING:
      this->handle_state_enabling_();
      break;
    case ModemComponentState::DISABLED:
      this->handle_state_disabled_();
      break;
    case ModemComponentState::POWERING_ON:
      this->handle_state_powering_on_();
      break;
    case ModemComponentState::SYNCING:
      this->handle_state_syncing_();
      break;
    case ModemComponentState::INIT_NETWORK:
      this->handle_state_init_network_();
      break;
    case ModemComponentState::START_PPP:
      this->handle_state_start_ppp_();
      break;
    case ModemComponentState::WAIT_IP:
      this->handle_state_wait_ip_();
      break;
    case ModemComponentState::CONNECTED:
      this->handle_state_connected_();
      break;
    case ModemComponentState::DISCONNECTED:
      this->handle_state_disconnected_();
      break;
    case ModemComponentState::NOT_RESPONDING:
      this->handle_state_not_responding_();
      break;
    case ModemComponentState::DISABLING:
      this->handle_state_disabling_();
      break;
    case ModemComponentState::POWERING_OFF:
      this->handle_state_powering_off_();
      break;
  }
}

void ModemComponent::handle_state_disabled_() {
  // Just wait 'enable()'
  if (this->disable_wanted_) {
    cancel_timeout("modem timeout");
    // this->disable_loop();
  } else {
    // Disable state was temporary (reset wanted)
    this->enable();
  }
}

void ModemComponent::handle_state_enabling_() {
  // Check modem state with status pin or autodetect.
  // And set the component state accordingly.

  if (this->modem_handler->status_pin) {
    // Check status pin for power state.
    if (!this->modem_handler->get_power_status()) {
      ESP_LOGV(TAG, "Modem OFF (status pin LOW).");
      this->component_state_ = ModemComponentState::POWERING_ON;
      return;
    }
  }

  auto try_autobaud = [&](int baud) {
    this->modem_handler->modem_create_dte_dce(baud);
    this->modem_handler->dce->set_mode(esp_modem::modem_mode::AUTODETECT);
    bool success = this->modem_handler->dce->get_mode() != esp_modem::modem_mode::UNDEF;
    if (success) {
      this->modem_handler->current_baud_rate = baud;
      this->modem_restore_state_.baud_rate = baud;
      this->pref_.save(&this->modem_restore_state_);
    }
    return success;
  };

  std::vector<int> bauds = {this->modem_handler->current_baud_rate, this->modem_restore_state_.baud_rate,
                            this->modem_handler->baud_rate, 115200};
  std::sort(bauds.begin(), bauds.end());
  bauds.erase(std::unique(bauds.begin(), bauds.end()), bauds.end());

  for (int b : bauds) {
    if (try_autobaud(b)) {
      ESP_LOGV(TAG, "Modem ON. Autodetect mode: %s, baud: %d",
               modem_mode_to_string(this->modem_handler->dce->get_mode()).c_str(), b);
      auto mode = this->modem_handler->dce->get_mode();
      if (mode == modem_mode::CMUX_MANUAL_MODE || mode == modem_mode::DATA_MODE) {
        if (b != this->modem_handler->baud_rate) {
          ESP_LOGI(TAG, "Modem connected, but baud rate has changed");
          if (mode == modem_mode::CMUX_MANUAL_MODE) {
            this->modem_handler->dce->set_mode(modem_mode::CMUX_MANUAL_EXIT);
          } else {
            this->modem_handler->dce->set_mode(modem_mode::COMMAND_MODE);
          }
          this->component_state_ = ModemComponentState::SYNCING;
          return;
        }
        // this->component_state_ = ModemComponentState::WAIT_IP;
        this->modem_handler->dce->set_mode(modem_mode::CMUX_MANUAL_EXIT);
        this->modem_handler->dce->set_mode(modem_mode::COMMAND_MODE);
        this->component_state_ = ModemComponentState::SYNCING;
        return;
      }
      this->component_state_ = ModemComponentState::SYNCING;
      return;
    }
  }

  if (this->modem_handler->power_pin) {
    ESP_LOGD(TAG, "Modem not responding, powering on...");
    this->component_state_ = ModemComponentState::POWERING_ON;
  } else {
    ESP_LOGE(TAG, "Unable enable modem, and no power pin defined.");
    this->component_state_ = ModemComponentState::NOT_RESPONDING;
  }
}

void ModemComponent::handle_state_powering_on_() {
  this->modem_handler->power_pin->digital_write(false);
  delay(this->modem_handler->power_ton_pulse_delay);
  this->modem_handler->power_pin->digital_write(true);
  uint32_t loop_delay = this->modem_handler->power_ton_delay;
  this->loop_delay_(loop_delay);
  ESP_LOGD(TAG, "Modem ON in %.1fs...", float(loop_delay) / 1000);

  this->component_state_ = ModemComponentState::ENABLING;
}

void ModemComponent::handle_state_powering_off_() {
  this->modem_handler->power_pin->digital_write(false);
  delay(this->modem_handler->power_toff_pulse_delay);
  this->modem_handler->power_pin->digital_write(true);
  this->loop_delay_(this->modem_handler->power_toff_delay);
  ESP_LOGD(TAG, "Modem should be OFF in %.1fs...", float(this->modem_handler->power_toff_delay) / 1000);

  this->component_state_ = ModemComponentState::DISABLED;
  this->modem_restore_state_.baud_rate = 0;
  this->pref_.save(&this->modem_restore_state_);
}

void ModemComponent::handle_state_syncing_() {
  if (this->modem_handler->dce->sync() != esp_modem::command_result::OK) {
    if (this->modem_handler->dce->set_mode(esp_modem::modem_mode::COMMAND_MODE)) {
      ESP_LOGD(TAG, "Modem set to COMMAND_MODE");
    } else {
      ESP_LOGE(TAG, "Failed sync modem");
      this->component_state_ = ModemComponentState::NOT_RESPONDING;
      return;
    }
  }

  if (this->modem_handler->baud_rate != this->modem_handler->current_baud_rate) {
    ESP_LOGD(TAG, "Setting baud rate: %d -> %d", this->modem_handler->current_baud_rate,
             this->modem_handler->baud_rate);
    this->modem_handler->dce->sync();
    if (this->modem_handler->dce->set_baud(this->modem_handler->baud_rate) == esp_modem::command_result::OK) {
      ESP_LOGD(TAG, "Modem baud rate set to %d.", this->modem_handler->baud_rate);
      delay(200);  // NOLINT
      this->modem_handler->modem_create_dte_dce(this->modem_handler->baud_rate);
      App.feed_wdt();
      delay(200);  // NOLINT
      this->modem_handler->dce->sync();
      if (this->modem_handler->dce->sync() == esp_modem::command_result::OK) {
        ESP_LOGI(TAG, "Modem synced at baud rate %d.", this->modem_handler->current_baud_rate);
        this->modem_restore_state_.baud_rate = this->modem_handler->current_baud_rate;
        this->pref_.save(&this->modem_restore_state_);
        global_preferences->sync();
      } else {
        ESP_LOGE(TAG, "Failed to sync modem at baud rate %d.", this->modem_handler->baud_rate);
        this->component_state_ = ModemComponentState::NOT_RESPONDING;
      }
    } else {
      ESP_LOGW(TAG, "Failed to set modem baud rate to %d. Using %d.", this->modem_handler->baud_rate,
               this->modem_handler->current_baud_rate);
      this->loop_delay_(1000);
      return;
    }
  }

  if (this->modem_handler->dce->sync() == esp_modem::command_result::OK) {
    ESP_LOGD(TAG, "Modem synced");
    this->modem_handler->send_init_at();
    this->component_state_ = ModemComponentState::INIT_NETWORK;
  }
}

void ModemComponent::handle_state_init_network_() {
  if (this->modem_handler->dce->sync() != esp_modem::command_result::OK) {
    ESP_LOGW(TAG, "Modem not synced during network init");
    this->component_state_ = ModemComponentState::SYNCING;
    return;
  }

  this->modem_handler->dce->set_radio_state(1);
  this->modem_handler->prepare_sim();
  this->modem_handler->dce->set_network_attachment_state(1);

  int attachement_state = 0;
  this->modem_handler->dce->get_network_attachment_state(attachement_state);

  if (attachement_state) {
    this->component_state_ = ModemComponentState::START_PPP;
    ESP_LOGI(TAG, "Modem initialized and ready");
  } else {
    ESP_LOGW(TAG, "Modem not yet ready to connect");
    this->modem_handler->modem_log_status();
    this->loop_delay_(4000);
  }
}

void ModemComponent::handle_state_start_ppp_() {
  this->status_set_warning("Starting connection");

  // ESP_LOGI(TAG, "%s", this->modem_handler->modem_network_status_string().c_str());
  this->modem_handler->modem_log_status();

  bool status = false;
  if (this->modem_handler->cmux) {
    status = this->modem_handler->dce->set_mode(esp_modem::modem_mode::CMUX_MODE);
  } else {
    status = this->modem_handler->dce->set_mode(esp_modem::modem_mode::DATA_MODE);
  }

  if (!status) {
    ESP_LOGE(TAG, "Failed to enter PPP. Resetting modem.");
    this->reset();
    this->loop_delay_(1000);
  } else {
    this->component_state_ = ModemComponentState::WAIT_IP;
  }
}

void ModemComponent::handle_state_wait_ip_() {
  static uint8_t retry = 10;
  // In WAIT_IP state, we wait for IP_EVENT_PPP_GOT_IP.
  if (this->modem_handler->network_infos.got_ip) {
    this->component_state_ = ModemComponentState::CONNECTED;
    this->status_clear_warning();
    this->dump_connect_params_();
    retry = 10;
    return;
  } else {
    if (--retry > 0) {
      ESP_LOGD(TAG, "Wait IP left retry: %d", retry);
      this->loop_delay_(this->modem_handler->connect_retry_delay);
    } else {
      ESP_LOGE(TAG, "Unable to get IP address");
      retry = 10;
      this->component_state_ = ModemComponentState::SYNCING;
    }
  }
}

void ModemComponent::handle_state_connected_() {
  cancel_timeout("modem timeout");
  if (!this->modem_handler->network_infos.got_ip) {
    ESP_LOGW(TAG, "Lost IP");
    this->component_state_ = ModemComponentState::DISCONNECTED;
    this->loop_delay_(this->modem_handler->connect_retry_delay);
    return;
  }
  // If CMUX, we can log status
  if (this->modem_handler->cmux) {
    if ((millis() - this->last_health_check_) > 30000) {
      this->last_health_check_ = millis();
      this->modem_handler->modem_log_status();
    }
  }
  this->loop_delay_(2000);
}

void ModemComponent::handle_state_disconnected_() {
  if (this->modem_handler->dce->sync() != esp_modem::command_result::OK) {
    ESP_LOGD(TAG, "Disconnected and not responding");
    this->component_state_ = ModemComponentState::NOT_RESPONDING;
  } else {
    ESP_LOGW(TAG, "Disconnected. Attempting to reconnect");
    this->component_state_ = ModemComponentState::START_PPP;
  }
}

void ModemComponent::handle_state_not_responding_() {
  // In NOT_RESPONDING state, we attempt recovery.
  if (this->modem_handler->status_pin && !this->modem_handler->get_power_status()) {
    ESP_LOGD(TAG, "Modem off, powering on");
    this->component_state_ = ModemComponentState::POWERING_ON;
  } else {
    ESP_LOGW(TAG, "Modem not responding, attempting a reset");
    this->reset();
  }
}

void ModemComponent::handle_state_disabling_() {
  ESP_LOGI(TAG, "Disabling modem");
  if (this->modem_handler->power_pin) {
    this->component_state_ = ModemComponentState::POWERING_OFF;
  } else {
    if (this->modem_handler->dce->get_mode() != esp_modem::modem_mode::COMMAND_MODE) {
      this->modem_handler->dce->set_mode(esp_modem::modem_mode::COMMAND_MODE);
    }
    if (this->modem_handler->dce->set_radio_state(0) == esp_modem::command_result::OK) {
      ESP_LOGI(TAG, "No power pin. Modem set to minimal functionality.");
    } else {
      ESP_LOGE(TAG, "Failed to set modem to minimal functionality.");
    }
    this->component_state_ = ModemComponentState::DISABLED;
    if (!this->disable_wanted_) {
      this->enable();
      this->disable_wanted_ = true;
    }
  }
  this->loop_delay_(this->modem_handler->command_delay);
}

void ModemComponent::abort_(const std::string &message) {
  ESP_LOGE(TAG, "Aborting: %s.", message.c_str());
  this->pref_.save(&this->modem_restore_state_);
  App.reboot();
}

void ModemComponent::loop_delay_(uint32_t delay_ms) { this->next_loop_millis_ = millis() + delay_ms; }

void ModemComponent::dump_connect_params_() {
  if (this->component_state_ != ModemComponentState::CONNECTED) {
    ESP_LOGCONFIG(TAG, "Modem connection: Not connected.");
    return;
  }
  esp_netif_ip_info_t ip = this->modem_handler->network_infos.ip_info;
  esp_netif_dns_info_t dns_main = this->modem_handler->network_infos.dns_main;
  esp_netif_dns_info_t dns_backup = this->modem_handler->network_infos.dns_backup;

  ESP_LOGCONFIG(TAG, "Modem connection:");
  ESP_LOGCONFIG(TAG, "  IP Address  : %s", network::IPAddress(&ip.ip).str().c_str());
  ESP_LOGCONFIG(TAG, "  Hostname    : '%s'", App.get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Subnet      : %s", network::IPAddress(&ip.netmask).str().c_str());
  ESP_LOGCONFIG(TAG, "  Gateway     : %s", network::IPAddress(&ip.gw).str().c_str());
  ESP_LOGCONFIG(TAG, "  DNS main    : %s", network::IPAddress(&dns_main.ip.u_addr.ip4).str().c_str());
  ESP_LOGCONFIG(TAG, "  DNS backup  : %s", network::IPAddress(&dns_backup.ip.u_addr.ip4).str().c_str());
}

}  // namespace modem
}  // namespace esphome

#endif
