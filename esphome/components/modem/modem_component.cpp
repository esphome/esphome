#ifdef USE_ESP32
#include "modem_component.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_netif_ppp.h>

#include <cinttypes>
#include <cstring>

namespace esphome::modem {

ESPHOME_LOG_TAG(TAG, "modem");

ModemComponent *global_modem_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static constexpr uint32_t COMMAND_TIMEOUT_MS = 1000;
static constexpr uint32_t WAIT_IP_TIMEOUT_MS = 50000;
static constexpr uint32_t RETRY_DELAY_MS = 1000;
static constexpr uint8_t UART_EVENT_QUEUE_SIZE = 10;
static constexpr size_t UART_EVENT_TASK_STACK_SIZE = 4096;
static constexpr uint8_t UART_EVENT_TASK_PRIORITY = 5;
static constexpr uint32_t BAUD_PREF_HASH = 76007670UL;
static constexpr int FALLBACK_BAUD_RATE = 115200;

ModemComponent::ModemComponent() {
  assert(global_modem_component == nullptr);
  global_modem_component = this;
}

void ModemComponent::enable() {
  ESP_LOGI(TAG, "Setting modem target state to CONNECTED");
  this->target_state_ = ModemComponentState::MODEM_CONNECTED;
  this->pin_sent_ = false;
  this->enable_loop();
}

void ModemComponent::disable() {
  ESP_LOGI(TAG, "Setting modem target state to DISABLED");
  this->target_state_ = ModemComponentState::MODEM_DISABLED;
}

network::IPAddresses ModemComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  esp_netif_ip_info_t ip;
  if (this->is_connected() && esp_netif_get_ip_info(this->ppp_netif_, &ip) == ESP_OK) {
    addresses[0] = network::IPAddress(&ip.ip);
  }
  return addresses;
}

void ModemComponent::setup() {
  this->pref_ = global_preferences->make_preference<int>(BAUD_PREF_HASH);
  this->pref_.load(&this->restored_baud_);

#ifdef UART_NUM_2
  for (auto p : {UART_NUM_2, UART_NUM_1}) {
#else
  for (auto p : {UART_NUM_1}) {
#endif
    if (!uart_is_driver_installed(p)) {
      this->uart_port_ = p;
      break;
    }
  }
  if (this->uart_port_ == UART_NUM_MAX) {
    this->mark_failed(LOG_STR("No free UART port for modem"));
    return;
  }

  if (CONFIG_ESP_TASK_WDT_TIMEOUT_S <= 10) {
    ESP_LOGW(TAG, "WDT timeout (%d s) may be too low for modem. Increase if WDT triggers.",
             CONFIG_ESP_TASK_WDT_TIMEOUT_S);
  }

  esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
  this->ppp_netif_ = esp_netif_new(&netif_ppp_config);
  assert(this->ppp_netif_);

  if (esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ModemComponent::ip_event_handler, this) != ESP_OK) {
    this->mark_failed(LOG_STR("IP event handler register failed"));
    return;
  }

  this->create_dte_dce_(this->baud_rate_);
}

void ModemComponent::dump_config() {
  ESP_LOGCONFIG(
      TAG,
      "Modem:\n"
      "  Model: %s\n"
      "  APN: %s\n"
      "  PIN code: %s\n"
      "  TX Pin: GPIO%u\n"
      "  RX Pin: GPIO%u\n"
      "  UART port: %d\n"
      "  Baud rate: %d\n"
      "  TX buffer size: %u\n"
      "  RX buffer size: %u\n"
      "  DTE buffer size: %u\n"
      "  Reboot timeout: %" PRIu32 " ms\n"
      "  Enabled: %s",
      this->model_, this->apn_, this->pin_code_ == nullptr ? LOG_STR_LITERAL("No") : LOG_STR_LITERAL("Yes (not shown)"),
      this->tx_pin_->get_pin(), this->rx_pin_->get_pin(), this->uart_port_, this->baud_rate_, this->tx_buffer_size_,
      this->rx_buffer_size_, this->dte_buffer_size_, this->timeout_, YESNO(this->is_enabled()));
  this->dump_connect_params_();
}

void ModemComponent::loop() {
  // Some modem steps need a pause; the early return alone defers the work
  if ((int32_t) (App.get_loop_component_start_time() - this->next_loop_millis_) < 0) {
    return;
  }
  this->transition_to_(this->compute_next_state_());
}

void ModemComponent::loop_delay_(uint32_t delay_ms) {
  this->next_loop_millis_ = App.get_loop_component_start_time() + delay_ms;
}

ModemComponentState ModemComponent::compute_next_state_() {
  if (this->target_state_ == ModemComponentState::MODEM_DISABLED) {
    if (this->component_state_ == ModemComponentState::MODEM_DISABLED) {
      // Parked until enable() turns the loop back on
      this->disable_loop();
      return ModemComponentState::MODEM_DISABLED;
    }
    if (this->component_state_ != ModemComponentState::MODEM_DISCONNECTING) {
      return ModemComponentState::MODEM_DISCONNECTING;
    }
  }

  switch (this->component_state_) {
    case ModemComponentState::MODEM_DISABLED:
      return ModemComponentState::MODEM_ENABLING;
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
  return this->component_state_;
}

void ModemComponent::create_dte_dce_(int baud_rate) {
  this->current_baud_rate_ = baud_rate;

  esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
  dte_config.uart_config.tx_io_num = this->tx_pin_->get_pin();
  dte_config.uart_config.rx_io_num = this->rx_pin_->get_pin();
  dte_config.uart_config.rx_buffer_size = this->rx_buffer_size_;
  dte_config.uart_config.tx_buffer_size = this->tx_buffer_size_;
  dte_config.uart_config.event_queue_size = UART_EVENT_QUEUE_SIZE;
  dte_config.uart_config.port_num = this->uart_port_;
  if (baud_rate != 0) {
    dte_config.uart_config.baud_rate = baud_rate;
  }
  if (this->rts_pin_ != nullptr) {
    dte_config.uart_config.rts_io_num = this->rts_pin_->get_pin();
    dte_config.uart_config.cts_io_num = this->cts_pin_->get_pin();
    dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_HW;
  } else {
    dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_NONE;
  }
  dte_config.task_stack_size = UART_EVENT_TASK_STACK_SIZE;
  dte_config.task_priority = UART_EVENT_TASK_PRIORITY;
  dte_config.dte_buffer_size = this->dte_buffer_size_;

  this->dce_.reset();
  this->dte_.reset();
  this->dte_ = create_uart_dte(&dte_config);
  esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(this->apn_);
  this->dce_ = this->dce_factory_(&dce_config, this->dte_, this->ppp_netif_);
}

bool ModemComponent::try_baud_(int baud) {
  this->create_dte_dce_(baud);
  this->dce_->set_mode(modem_mode::AUTODETECT);
  if (this->dce_->get_mode() != modem_mode::UNDEF) {
    return true;
  }
  // Some modems ignore the autobaud probe, so confirm with a plain AT
  App.feed_wdt();
  return this->send_at("AT", 100).success;
}

ModemComponentState ModemComponent::handle_state_enabling_() {
  // Find the baud rate the modem answers at and leave it in command mode, synced
  this->dce_->set_mode(modem_mode::AUTODETECT);
  auto mode = this->dce_->get_mode();
  ESP_LOGI(TAG, "Autodetect mode: %d", static_cast<int>(mode));

  switch (mode) {
    case modem_mode::CMUX_MANUAL_MODE:
      ESP_LOGD(TAG, "Modem warm reboot. Exiting CMUX");
      this->dce_->set_mode(modem_mode::CMUX_MANUAL_EXIT);
      break;

    case modem_mode::UNDEF: {
      // One candidate per pass keeps each loop() pass short
      const int bauds[] = {this->current_baud_rate_, this->restored_baud_, this->baud_rate_, FALLBACK_BAUD_RATE};
      const int baud = bauds[this->baud_index_++ % 4];
      if (baud == 0) {
        return ModemComponentState::MODEM_ENABLING;
      }
      if (!this->try_baud_(baud)) {
        ESP_LOGW(TAG, "Modem not responding at %d baud. Check power, PWRKEY and TX/RX wiring.", baud);
        this->loop_delay_(RETRY_DELAY_MS);
        return ModemComponentState::MODEM_ENABLING;
      }
      ESP_LOGD(TAG, "Modem responded at baud %d", baud);
      break;
    }

    case modem_mode::COMMAND_MODE:
      break;

    case modem_mode::DATA_MODE:
      // Can block 20 s; esp_modem 2.0 offers no shorter exit
      ESP_LOGW(TAG, "Exiting DATA_MODE (long blocking call, make sure to have a long WDT)");
      this->dce_->set_mode(modem_mode::COMMAND_MODE);
      break;

    default:
      ESP_LOGW(TAG, "Modem in unexpected mode %d. Switching to COMMAND mode.", static_cast<int>(mode));
      this->dce_->set_mode(modem_mode::COMMAND_MODE);
      this->dce_->set_mode(modem_mode::UNDEF);
      break;
  }

  if (this->dce_->sync() != command_result::OK) {
    ESP_LOGW(TAG, "Waiting SYNC");
    this->loop_delay_(RETRY_DELAY_MS);
    return ModemComponentState::MODEM_ENABLING;
  }

#ifdef USE_MODEM_URC
  this->dce_->set_enhanced_urc([this](const DTE::UrcBufferInfo &buffer_info) {
    if (!buffer_info.is_command_active) {
      // Publish from the main loop, not from the UART task
      this->defer(
          [this, line = std::string(reinterpret_cast<const char *>(buffer_info.new_data_start),
                                    buffer_info.new_data_size)]() { this->urc_text_sensor_->publish_state(line); });
    }
    return DTE::UrcConsumeInfo{DTE::UrcConsumeResult::CONSUME_NONE, 0};
  });
#endif

  int radio_state = 1;
  this->dce_->get_radio_state(radio_state);
  if (radio_state == 1) {
    // On warm reboot, drop the previous network session first
    if (this->dce_->set_radio_state(0) != command_result::OK) {
      ESP_LOGW(TAG, "Failed to disconnect previous network session");
    }
    this->loop_delay_(RETRY_DELAY_MS);
    return ModemComponentState::MODEM_ENABLING;
  }

  if (this->baud_rate_ != 0 && this->baud_rate_ != this->current_baud_rate_) {
    ESP_LOGD(TAG, "Setting baud rate: %d -> %d", this->current_baud_rate_, this->baud_rate_);
    if (this->dce_->set_baud(this->baud_rate_) != command_result::OK) {
      ESP_LOGW(TAG, "Failed to set modem baud rate to %d. Using %d.", this->baud_rate_, this->current_baud_rate_);
      this->loop_delay_(RETRY_DELAY_MS);
      return ModemComponentState::MODEM_ENABLING;
    }
    delay(200);  // NOLINT
    this->create_dte_dce_(this->baud_rate_);
    App.feed_wdt();
    delay(200);  // NOLINT
    if (this->dce_->sync() != command_result::OK) {
      ESP_LOGE(TAG, "Failed to sync modem at baud rate %d.", this->baud_rate_);
      return ModemComponentState::MODEM_DISCONNECTING;
    }
    ESP_LOGI(TAG, "Modem synced at baud rate %d.", this->current_baud_rate_);
  }

  if (this->restored_baud_ != this->current_baud_rate_) {
    this->restored_baud_ = this->current_baud_rate_;
    this->pref_.save(&this->restored_baud_);
  }
  return ModemComponentState::MODEM_SYNCED;
}

ModemComponentState ModemComponent::handle_state_synced_() {
  for (const char *cmd : this->init_at_commands_) {
    App.feed_wdt();
    this->send_at(cmd, 0, true);
  }
  return ModemComponentState::MODEM_CONNECTING;
}

ModemComponentState ModemComponent::handle_state_connecting_() {
  if (this->dce_->sync() != command_result::OK) {
    ESP_LOGW(TAG, "Modem not synced during network init");
    return ModemComponentState::MODEM_ENABLING;
  }

  this->dce_->set_radio_state(1);
  switch (this->prepare_sim_()) {
    case SimResult::SIM_RESULT_READY:
      break;
    case SimResult::SIM_RESULT_RETRY:
      this->loop_delay_(COMMAND_TIMEOUT_MS);
      return ModemComponentState::MODEM_CONNECTING;
    case SimResult::SIM_RESULT_FATAL:
      // Retrying would burn the remaining PIN attempts; stay off until the user fixes the SIM
      this->status_set_error(LOG_STR("SIM locked"));
      this->target_state_ = ModemComponentState::MODEM_DISABLED;
      return ModemComponentState::MODEM_DISCONNECTING;
  }
  this->dce_->set_network_attachment_state(1);

  int attached = 0;
  this->dce_->get_network_attachment_state(attached);
  if (!attached) {
    this->log_status_();
    this->loop_delay_(4 * RETRY_DELAY_MS);
    return ModemComponentState::MODEM_CONNECTING;
  }

  ESP_LOGI(TAG, "Modem initialized and ready, starting PPP");
  if (!this->dce_->set_mode(modem_mode::CMUX_MODE)) {
    ESP_LOGE(TAG, "Failed to enter PPP. Resetting modem.");
    return ModemComponentState::MODEM_DISCONNECTING;
  }
  return ModemComponentState::MODEM_WAIT_IP;
}

ModemComponentState ModemComponent::handle_state_wait_ip_() {
  if (this->got_ip_.load(std::memory_order_acquire)) {
    return ModemComponentState::MODEM_CONNECTED;
  }
  if (App.get_loop_component_start_time() - this->wait_ip_start_ < WAIT_IP_TIMEOUT_MS) {
    return ModemComponentState::MODEM_WAIT_IP;
  }
  ESP_LOGE(TAG, "Unable to get IP address");
  return ModemComponentState::MODEM_DISCONNECTING;
}

ModemComponentState ModemComponent::handle_state_connected_() {
  if (!this->got_ip_.load(std::memory_order_acquire)) {
    ESP_LOGW(TAG, "Lost IP");
    return ModemComponentState::MODEM_DISCONNECTING;
  }
  return ModemComponentState::MODEM_CONNECTED;
}

ModemComponentState ModemComponent::handle_state_disconnecting_() {
  this->dce_->set_mode(modem_mode::CMUX_MANUAL_EXIT);
  // Turn the radio off; waiting for the lost IP event could take up to 2 min
  if (this->dce_->set_radio_state(0) == command_result::OK) {
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
  // on_disconnect pairs with on_connect, so a failed attempt that never connected does not fire it
  if (next_state != ModemComponentState::MODEM_DISCONNECTING ||
      previous_state == ModemComponentState::MODEM_CONNECTED) {
    this->on_state_callback_.call(next_state);
  }
  this->on_enter_state_(next_state);
}

void ModemComponent::on_enter_state_(ModemComponentState state) {
  switch (state) {
    case ModemComponentState::MODEM_ENABLING:
      if (this->timeout_ != 0) {
        this->set_timeout(CONNECT_TIMEOUT_ID, this->timeout_,
                          [this]() { this->abort_("Modem was not able to connect (timeout)"); });
      }
      break;
    case ModemComponentState::MODEM_WAIT_IP:
      this->wait_ip_start_ = App.get_loop_component_start_time();
      break;
    case ModemComponentState::MODEM_CONNECTED:
      this->cancel_timeout(CONNECT_TIMEOUT_ID);
      this->status_clear_warning();
      this->dump_connect_params_();
      break;
    case ModemComponentState::MODEM_DISCONNECTING:
      ESP_LOGI(TAG, "Disconnecting modem");
      this->cancel_timeout(CONNECT_TIMEOUT_ID);
      this->loop_delay_(COMMAND_TIMEOUT_MS);
      break;
    default:
      break;
  }
}

void ModemComponent::abort_(const char *message) {
  ESP_LOGE(TAG, "Aborting: %s.", message);
  App.reboot();
}

AtCommandResult ModemComponent::send_at(const std::string &cmd, uint32_t timeout, bool verbose) {
  AtCommandResult result;
  if (!this->dce_) {
    ESP_LOGE(TAG, "Modem DCE not ready, cannot send AT command: %s", cmd.c_str());
    return result;
  }
  result.success =
      this->dce_->at(cmd, result.output, timeout == 0 ? COMMAND_TIMEOUT_MS : timeout) == command_result::OK;
  if (verbose) {
    ESP_LOGI(TAG, "AT '%s' -> %s: %s", cmd.c_str(), result.success ? LOG_STR_LITERAL("OK") : LOG_STR_LITERAL("FAILED"),
             result.output.c_str());
  }
  return result;
}

SimResult ModemComponent::prepare_sim_() {
  AtCommandResult result = this->send_at("AT+CPIN?");
  if (!result.success) {
    ESP_LOGW(TAG, "Failed to check pin: %s", result.output.c_str());
    return SimResult::SIM_RESULT_RETRY;
  }
  const char *status = result.output.c_str();
  if (result.output.starts_with("+CPIN: ")) {
    status += 7;
  }
  ESP_LOGD(TAG, "SIM: %s", status);

  if (strstr(status, "READY") != nullptr) {
    return SimResult::SIM_RESULT_READY;
  }
  if (strstr(status, "PUK") != nullptr) {
    ESP_LOGE(TAG, "SIM is PUK locked; unlock it in a phone");
    return SimResult::SIM_RESULT_FATAL;
  }
  if (strstr(status, "PIN") == nullptr) {
    return SimResult::SIM_RESULT_RETRY;  // e.g. the SIM is still initializing
  }
  if (this->pin_code_ == nullptr) {
    ESP_LOGE(TAG, "SIM requires a PIN but pin_code is not set");
    return SimResult::SIM_RESULT_FATAL;
  }
  if (this->pin_sent_) {
    ESP_LOGE(TAG, "SIM still locked after sending the PIN; check pin_code");
    return SimResult::SIM_RESULT_FATAL;
  }
  this->pin_sent_ = true;
  ESP_LOGD(TAG, "Unlocking SIM with PIN...");
  if (this->dce_->set_pin(this->pin_code_) != command_result::OK) {
    ESP_LOGE(TAG, "The modem rejected the PIN; check pin_code");
    return SimResult::SIM_RESULT_FATAL;
  }
  return SimResult::SIM_RESULT_RETRY;  // AT+CPIN? confirms READY on the next pass
}

void ModemComponent::log_status_() {
  int rssi = 99, ber = 99, registration = 0, mode = 0, cfun = -1;
  this->dce_->get_radio_state(cfun);
  this->dce_->get_network_registration_state(registration);
  this->dce_->get_signal_quality(rssi, ber);
  this->dce_->get_network_system_mode(mode);
  ESP_LOGW(TAG, "Modem not attached yet: registration %d, radio %d, network type %d, CSQ %d, BER %d", registration,
           cfun, mode, rssi, ber);
}

void ModemComponent::ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  auto *self = static_cast<ModemComponent *>(arg);
  if (event_id == IP_EVENT_PPP_GOT_IP) {
    auto *event = static_cast<ip_event_got_ip_t *>(event_data);
    ESP_LOGD(TAG, "PPP IP event: Got IP " IPSTR, IP2STR(&event->ip_info.ip));
    self->got_ip_.store(true, std::memory_order_release);
  } else if (event_id == IP_EVENT_PPP_LOST_IP) {
    ESP_LOGD(TAG, "PPP IP event: Lost IP");
    self->got_ip_.store(false, std::memory_order_release);
  }
}

void ModemComponent::dump_connect_params_() {
  esp_netif_ip_info_t ip;
  esp_netif_dns_info_t dns_main;
  esp_netif_dns_info_t dns_backup;
  if (!this->is_connected() || esp_netif_get_ip_info(this->ppp_netif_, &ip) != ESP_OK) {
    ESP_LOGCONFIG(TAG, "  Connection: not connected");
    return;
  }
  esp_netif_get_dns_info(this->ppp_netif_, ESP_NETIF_DNS_MAIN, &dns_main);
  esp_netif_get_dns_info(this->ppp_netif_, ESP_NETIF_DNS_BACKUP, &dns_backup);
  char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
  char netmask_buf[network::IP_ADDRESS_BUFFER_SIZE];
  char gw_buf[network::IP_ADDRESS_BUFFER_SIZE];
  char dns_main_buf[network::IP_ADDRESS_BUFFER_SIZE];
  char dns_backup_buf[network::IP_ADDRESS_BUFFER_SIZE];
  ESP_LOGCONFIG(TAG,
                "  Connection:\n"
                "    IP Address: %s\n"
                "    Hostname: '%s'\n"
                "    Subnet: %s\n"
                "    Gateway: %s\n"
                "    DNS main: %s\n"
                "    DNS backup: %s",
                network::IPAddress(&ip.ip).str_to(ip_buf), App.get_name().c_str(),
                network::IPAddress(&ip.netmask).str_to(netmask_buf), network::IPAddress(&ip.gw).str_to(gw_buf),
                network::IPAddress(&dns_main.ip.u_addr.ip4).str_to(dns_main_buf),
                network::IPAddress(&dns_backup.ip.u_addr.ip4).str_to(dns_backup_buf));
}

}  // namespace esphome::modem

#endif
