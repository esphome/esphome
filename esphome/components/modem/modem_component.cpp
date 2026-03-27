#ifdef USE_ESP32
#include "modem_component.h"
#include "modem_handler.h"

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
  ESP_LOGI(TAG, "Setting modem target state to CONNECTED");
  this->target_state_ = ModemComponentState::MODEM_CONNECTED;
  // State machine in loop() will handle the ascent
}

void ModemComponent::disable() {
  ESP_LOGI(TAG, "Setting modem target state to DISABLED");
  this->target_state_ = ModemComponentState::MODEM_DISABLED;
  // State machine in loop() will handle the descent
}

network::IPAddresses ModemComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  if (this->component_state_ == ModemComponentState::MODEM_CONNECTED) {
    addresses[0] = network::IPAddress(&this->modem_handler->network_infos.ip_info.ip);
  }
  return addresses;
}

void ModemComponent::setup() {
  ESP_LOGI(TAG, "Modem setup...");
  this->pref_ = global_preferences->make_preference<ModemRestoreState>(76007670UL);
  this->pref_.load(&this->modem_restore_state_);

  // find an available UART
#ifdef UART_NUM_2
  for (auto p : {UART_NUM_2, UART_NUM_1}) {
#else
  for (auto p : {UART_NUM_1}) {
#endif
    if (!uart_is_driver_installed(p)) {
      this->modem_handler->uart_port_num = p;
      break;
    }
  }
  if (this->modem_handler->uart_port_num == UART_NUM_MAX) {
    ESP_LOGE(TAG, "No free UART port for modem");
    this->mark_failed(LOG_STR("No free UART port for modem"));
    return;
  }

  ESP_LOGCONFIG(TAG, "Config Modem:");
  ESP_LOGCONFIG(TAG, "  Model     : %s", this->modem_handler->model.c_str());
  ESP_LOGCONFIG(TAG, "  APN       : %s", this->modem_handler->apn.c_str());
  ESP_LOGCONFIG(TAG, "  PIN code  : %s", (this->modem_handler->pin_code.empty()) ? "No" : "Yes (not shown)");
  ESP_LOGCONFIG(TAG, "  Tx Pin    : GPIO%u", this->modem_handler->tx_pin->get_pin());
  ESP_LOGCONFIG(TAG, "  Rx Pin    : GPIO%u", this->modem_handler->rx_pin->get_pin());
  ESP_LOGCONFIG(TAG, "  Enabled   : %s",
                (this->component_state_ != ModemComponentState::MODEM_DISABLED) ? "Yes" : "No");
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

  esp_err_t err;

  // This section should be removed once #14012 is merged.
  ESP_LOGV(TAG, "PPP netif init.");
  err = esp_netif_init();
  ESPHL_ERROR_CHECK(err, "PPP netif init failed");
  err = esp_event_loop_create_default();
  ESPHL_ERROR_CHECK(err, "PPP event loop init failed");
  // end of section to remove

  esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
  this->modem_handler->ppp_netif = esp_netif_new(&netif_ppp_config);
  assert(this->modem_handler->ppp_netif);

  err = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ModemHandler::ip_event_handler,
                                   this->modem_handler.get());
  ESPHL_ERROR_CHECK(err, "IP event handler register failed");

  this->modem_handler->modem_create_dte_dce(this->modem_handler->baud_rate);

  ESP_LOGV(TAG, "Setup complete");
}

void ModemComponent::loop() {
  if ((millis() < this->next_loop_millis_)) {
    // Some commands require a delay.
    delay(10);
    return;
  }

  // Calculate next step toward target_state_
  ModemComponentState next_state = this->compute_next_state_();

  if (next_state != this->component_state_) {
    this->transition_to_(next_state);
  }
}

ModemComponentState ModemComponent::compute_next_state_() {
  // Priority 1: If target = DISABLED and we're not there, descend
  if (this->target_state_ == ModemComponentState::MODEM_DISABLED) {
    if (this->component_state_ == ModemComponentState::MODEM_DISABLED) {
      return ModemComponentState::MODEM_DISABLED;  // Stay
    }
    if (this->component_state_ != ModemComponentState::MODEM_DISCONNECTING) {
      return ModemComponentState::MODEM_DISCONNECTING;  // Start descent
    }
    // Otherwise we're already in DISCONNECTING, let handler manage
  }

  // Priority 2: Execute current state logic
  switch (this->component_state_) {
    case ModemComponentState::MODEM_DISABLED:
      return this->handle_state_disabled_();
    case ModemComponentState::MODEM_ENABLING:
      return this->handle_state_enabling_();
    case ModemComponentState::MODEM_SYNCED:
      return this->handle_state_synced_();
    case ModemComponentState::MODEM_CONNECTING:
      return this->handle_state_connecting_();
    case ModemComponentState::MODEM_WAIT_IP:
      return this->handle_state_wait_ip_();
    case ModemComponentState::MODEM_CONNECTED:
      return this->handle_state_connected_();
    case ModemComponentState::MODEM_DISCONNECTING:
      return this->handle_state_disconnecting_();
  }

  return this->component_state_;  // Fallback
}

ModemComponentState ModemComponent::handle_state_disabled_() {
  this->loop_delay_(3000);

  // If target is DISABLED, stay here
  if (this->target_state_ == ModemComponentState::MODEM_DISABLED) {
    return ModemComponentState::MODEM_DISABLED;
  }

  // Otherwise, target is CONNECTED, start ascent
  return ModemComponentState::MODEM_ENABLING;
}

ModemComponentState ModemComponent::handle_state_enabling_() {
  // Connect to the modem (testing different baud), and set the modem in a fresh state (disconnect from a previous
  // connection) the modem will be synced at expected baud rate when exiting this state.

  auto try_autobaud = [&](int baud) {
    this->modem_handler->modem_create_dte_dce(baud);
    this->modem_handler->dce->set_mode(esp_modem::modem_mode::AUTODETECT);
    bool success = this->modem_handler->dce->get_mode() != esp_modem::modem_mode::UNDEF;
    // Sometimes the modem does not answer autobaud commands,
    // so try sending an AT command to confirm it's responsive at this baud rate.
    if (!success) {
      App.feed_wdt();
      auto result = this->modem_handler->send_at("AT", 100);
      success = result.esp_modem_command_result == command_result::OK;
    }
    if (success) {
      this->modem_handler->current_baud_rate = baud;
      this->modem_restore_state_.baud_rate = baud;
      this->pref_.save(&this->modem_restore_state_);
    }
    return success;
  };

  // possible baud rate to try
  std::vector<int> bauds = {this->modem_handler->current_baud_rate, this->modem_restore_state_.baud_rate,
                            this->modem_handler->baud_rate, 115200};
  std::sort(bauds.begin(), bauds.end());
  bauds.erase(std::unique(bauds.begin(), bauds.end()), bauds.end());

  this->modem_handler->dce->set_mode(esp_modem::modem_mode::AUTODETECT);

  auto mode = this->modem_handler->dce->get_mode();

  ESP_LOGI(TAG, "Autodetect mode (1st): %d", static_cast<int>(mode));

  switch (mode) {
    case modem_mode::CMUX_MANUAL_MODE:
      ESP_LOGD(TAG, "Modem warm reboot. Exiting CMUX");
      this->modem_handler->dce->set_mode(modem_mode::CMUX_MANUAL_EXIT);
      break;

    case modem_mode::UNDEF:
      ESP_LOGD(TAG, "Trying other baud rate");
      for (int b : bauds) {
        if (!try_autobaud(b)) {
          ESP_LOGD(TAG, "Modem responded at baud %d", b);
          return ModemComponentState::MODEM_ENABLING;
        }
      }

    case modem_mode::COMMAND_MODE:
      ESP_LOGD(TAG, "OK: Modem in COMMAND mode.");
      break;

    case modem_mode::DATA_MODE:
      // Can block 20s! But no workaround found (with esp_modem 2.0.0)
      ESP_LOGW(TAG, "Exiting DATA_MODE. (long blocking call, make sur to have a long WDT!)");
      this->modem_handler->dce->set_mode(modem_mode::COMMAND_MODE);
      break;

    default:
      ESP_LOGW(TAG, "Modem in unexpected mode %d. Attempting to switch to COMMAND mode.", static_cast<int>(mode));
      this->modem_handler->dce->set_mode(modem_mode::COMMAND_MODE);
      this->modem_handler->dce->set_mode(modem_mode::UNDEF);
      // return ModemComponentState::MODEM_ENABLING;
      break;
  }

  if (this->modem_handler->dce->sync() != esp_modem::command_result::OK) {
    ESP_LOGW(TAG, "Waiting SYNC");
    this->loop_delay_(1000);
    return ModemComponentState::MODEM_ENABLING;
  }

#ifdef USE_MODEM_URC
  auto urc_handler = [this](const esp_modem::DTE::UrcBufferInfo &buffer_info) {
    if (!buffer_info.is_command_active) {
      std::string line(reinterpret_cast<const char *>(buffer_info.new_data_start), buffer_info.new_data_size);
      // Publish to text sensor if configured
      if (this->urc_text_sensor != nullptr) {
        // defer so we won't be in the callback context
        this->defer([this, line]() { this->urc_text_sensor->publish_state(line); });
      }
    }
    return esp_modem::DTE::UrcConsumeInfo{esp_modem::DTE::UrcConsumeResult::CONSUME_NONE, 0};
  };
  this->modem_handler->dce->set_enhanced_urc(urc_handler);
#endif

  int radio_state = 1;
  this->modem_handler->dce->get_radio_state(radio_state);
  if (radio_state == 1) {
    // On warm reboot, we need to disconnect modem radio.
    if (this->modem_handler->dce->set_radio_state(0) != command_result::OK) {
      ESP_LOGW(TAG, "Failed to disconnect previous network session");
    }
    return ModemComponentState::MODEM_ENABLING;
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
        return ModemComponentState::MODEM_DISCONNECTING;
      }
    } else {
      ESP_LOGW(TAG, "Failed to set modem baud rate to %d. Using %d.", this->modem_handler->baud_rate,
               this->modem_handler->current_baud_rate);
      this->loop_delay_(1000);
      return ModemComponentState::MODEM_ENABLING;
    }
  }

  return ModemComponentState::MODEM_SYNCED;
}

ModemComponentState ModemComponent::handle_state_synced_() { return ModemComponentState::MODEM_CONNECTING; }

ModemComponentState ModemComponent::handle_state_connecting_() {
  // Merges INIT_NETWORK and START_PPP states
  if (this->modem_handler->dce->sync() != esp_modem::command_result::OK) {
    ESP_LOGW(TAG, "Modem not synced during network init");
    return ModemComponentState::MODEM_ENABLING;
  }

  int attachement_state = 0;
  this->modem_handler->dce->set_radio_state(1);
  this->modem_handler->prepare_sim();
  this->modem_handler->dce->set_network_attachment_state(1);

  this->modem_handler->dce->get_network_attachment_state(attachement_state);

  if (!attachement_state) {
    ESP_LOGW(TAG, "Modem not yet ready to connect");
    this->modem_handler->modem_log_status();
    this->loop_delay_(4000);
    return ModemComponentState::MODEM_CONNECTING;
  }

  ESP_LOGI(TAG, "Modem initialized and ready, starting PPP");

  if (!this->modem_handler->dce->set_mode(esp_modem::modem_mode::CMUX_MODE)) {
    ESP_LOGE(TAG, "Failed to enter PPP. Resetting modem.");
    return ModemComponentState::MODEM_DISCONNECTING;
  }

  return ModemComponentState::MODEM_WAIT_IP;
}

ModemComponentState ModemComponent::handle_state_wait_ip_() {
  if (this->modem_handler->network_infos.got_ip) {
    return ModemComponentState::MODEM_CONNECTED;
  }
  if (this->wait_ip_retry_ > 0) {
    --this->wait_ip_retry_;
  }
  if (this->wait_ip_retry_ > 0) {
    ESP_LOGD(TAG, "Wait IP left retry: %d", this->wait_ip_retry_);
    this->loop_delay_(this->modem_handler->connect_retry_delay);
    return ModemComponentState::MODEM_WAIT_IP;
  }
  ESP_LOGE(TAG, "Unable to get IP address");
  return ModemComponentState::MODEM_DISCONNECTING;
}

ModemComponentState ModemComponent::handle_state_connected_() {
  if (!this->modem_handler->network_infos.got_ip) {
    ESP_LOGW(TAG, "Lost IP");
    this->loop_delay_(this->modem_handler->connect_retry_delay);
    return ModemComponentState::MODEM_DISCONNECTING;
  }
  return ModemComponentState::MODEM_CONNECTED;
}

ModemComponentState ModemComponent::handle_state_disconnecting_() {
  this->modem_handler->dce->set_mode(modem_mode::CMUX_MANUAL_EXIT);

  // turn off radio.It can take up to 2 min to get lost IP event.
  if (this->modem_handler->dce->set_radio_state(0) == esp_modem::command_result::OK) {
    ESP_LOGI(TAG, "Modem set to minimal functionality.");
  } else {
    ESP_LOGE(TAG, "Failed to set modem to minimal functionality.");
  }
  return ModemComponentState::MODEM_DISABLED;
}

void ModemComponent::transition_to_(ModemComponentState next_state) {
  if (next_state == this->component_state_) {
    return;
  }
  ModemComponentState previous_state = this->component_state_;
  this->component_state_ = next_state;
  ESP_LOGD(TAG, "State change: %d -> %d", static_cast<int>(previous_state), static_cast<int>(next_state));
  this->on_state_callback_.call(next_state);
  this->on_enter_state_(next_state);
}

void ModemComponent::on_enter_state_(ModemComponentState state) {
  // State invariants (entry establishes these):
  // - MODEM_WAIT_IP: waiting for PPP got IP, retry counter initialized.
  // - MODEM_CONNECTED: got_ip expected, modem_timeout cancelled, params dumped.
  // - DISABLED: no modem timeouts active, no retries pending.
  switch (state) {
    case ModemComponentState::MODEM_ENABLING:
      set_timeout("modem_timeout", this->timeout_,
                  [this]() { this->abort_("Modem was not able to connect (timeout)"); });
      break;
    case ModemComponentState::MODEM_WAIT_IP:
      this->wait_ip_retry_ = 10;
      break;
    case ModemComponentState::MODEM_CONNECTED:
      cancel_timeout("modem_timeout");
      this->status_clear_warning();
      this->dump_connect_params_();
      break;
    case ModemComponentState::MODEM_DISCONNECTING:
      ESP_LOGI(TAG, "Disconnecting modem");
      cancel_timeout("modem_timeout");
      this->loop_delay_(this->modem_handler->command_delay);
      break;
    case ModemComponentState::MODEM_DISABLED:
      cancel_timeout("modem_timeout");
      this->wait_ip_retry_ = 0;
      break;
    default:
      break;
  }
}

void ModemComponent::abort_(const std::string &message) {
  ESP_LOGE(TAG, "Aborting: %s.", message.c_str());
  this->pref_.save(&this->modem_restore_state_);
  App.reboot();
}

void ModemComponent::loop_delay_(uint32_t delay_ms) { this->next_loop_millis_ = millis() + delay_ms; }

void ModemComponent::dump_connect_params_() {
  char buffer[network::IP_ADDRESS_BUFFER_SIZE];
  if (this->component_state_ != ModemComponentState::MODEM_CONNECTED) {
    ESP_LOGCONFIG(TAG, "Modem connection: Not connected.");
    return;
  }
  esp_netif_ip_info_t ip = this->modem_handler->network_infos.ip_info;
  esp_netif_dns_info_t dns_main = this->modem_handler->network_infos.dns_main;
  esp_netif_dns_info_t dns_backup = this->modem_handler->network_infos.dns_backup;

  ESP_LOGCONFIG(TAG, "Modem connection:");
  ESP_LOGCONFIG(TAG, "  IP Address  : %s", network::IPAddress(&ip.ip).str_to(buffer));
  ESP_LOGCONFIG(TAG, "  Hostname    : '%s'", App.get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Subnet      : %s", network::IPAddress(&ip.netmask).str_to(buffer));
  ESP_LOGCONFIG(TAG, "  Gateway     : %s", network::IPAddress(&ip.gw).str_to(buffer));
  ESP_LOGCONFIG(TAG, "  DNS main    : %s", network::IPAddress(&dns_main.ip.u_addr.ip4).str_to(buffer));
  ESP_LOGCONFIG(TAG, "  DNS backup  : %s", network::IPAddress(&dns_backup.ip.u_addr.ip4).str_to(buffer));
}

}  // namespace modem
}  // namespace esphome

#endif
