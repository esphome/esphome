#ifdef USE_ESP32
#include "modem_component.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_netif_ppp.h>

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome::modem {

ESPHOME_LOG_TAG(TAG, "modem");

ModemComponent *global_modem_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static constexpr uint32_t COMMAND_TIMEOUT_MS = 1000;
static constexpr uint32_t WAIT_IP_TIMEOUT_MS = 50000;
static constexpr uint8_t UART_EVENT_QUEUE_SIZE = 10;
static constexpr uint32_t BAUD_PREF_HASH = 76007670UL;
static constexpr int FALLBACK_BAUD_RATE = 115200;

ModemComponent::ModemComponent() { global_modem_component = this; }

void ModemComponent::enable() {
  ESP_LOGI(TAG, "Setting modem target state to CONNECTED");
  this->target_state_ = ModemComponentState::MODEM_CONNECTED;
  this->pin_sent_ = false;
  // A link that is already up has nothing to wait for; a lost one re-arms the timer itself
  if (this->component_state_ != ModemComponentState::MODEM_CONNECTED) {
    this->arm_connect_timeout_();
  }
  this->enable_loop();
}

void ModemComponent::disable() {
  ESP_LOGI(TAG, "Setting modem target state to DISABLED");
  this->target_state_ = ModemComponentState::MODEM_DISABLED;
  this->cancel_timeout(CONNECT_TIMEOUT_ID);
  // Also the reboot timer a failed setup armed outside the component
  App.scheduler.cancel_timeout(this);
}

void ModemComponent::arm_connect_timeout_() {
  // Measures time without a connection, like wifi, so retry cycles do not restart it
  if (this->timeout_ != 0) {
    this->set_timeout(CONNECT_TIMEOUT_ID, this->timeout_, []() {
      ESP_LOGE(TAG, "Modem was not able to connect (timeout), rebooting");
      App.reboot();
    });
  }
}

network::IPAddresses ModemComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  esp_netif_ip_info_t ip;
  if (this->is_connected() && esp_netif_get_ip_info(this->ppp_netif_, &ip) == ESP_OK) {
    addresses[0] = network::IPAddress(&ip.ip);
  }
  return addresses;
}

// A failed component's own timers are skipped, so the reboot watchdog is armed outside it
void ModemComponent::fail_setup_(const LogString *reason) {
  this->mark_failed(reason);
  if (this->timeout_ != 0 && this->target_state_ == ModemComponentState::MODEM_CONNECTED) {
    App.scheduler.set_timeout(this, this->timeout_, []() {
      ESP_LOGE(TAG, "Modem setup failed, rebooting");
      App.reboot();
    });
  }
}

void ModemComponent::setup() {
  this->pref_ = global_preferences->make_preference<int>(BAUD_PREF_HASH);
  this->pref_.load(&this->restored_baud_);

  // Highest free port first; UART0 stays with the logger
  for (int p = UART_NUM_MAX - 1; p > UART_NUM_0; p--) {
    if (!uart_is_driver_installed(static_cast<uart_port_t>(p))) {
      this->uart_port_ = static_cast<uart_port_t>(p);
      break;
    }
  }
  if (this->uart_port_ == UART_NUM_MAX) {
    this->mark_failed(LOG_STR("No free UART port for modem"));
    return;
  }

  esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
  this->ppp_netif_ = esp_netif_new(&netif_ppp_config);
  if (this->ppp_netif_ == nullptr) {
    this->fail_setup_(LOG_STR("PPP netif creation failed"));
    return;
  }

  auto ip_event = [](void *arg, esp_event_base_t, int32_t event_id, void *event_data) {
    auto *self = static_cast<ModemComponent *>(arg);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
      auto *event = static_cast<ip_event_got_ip_t *>(event_data);
      ESP_LOGD(TAG, "PPP IP event: Got IP " IPSTR, IP2STR(&event->ip_info.ip));
      self->got_ip_.store(true, std::memory_order_release);
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
      ESP_LOGD(TAG, "PPP IP event: Lost IP");
      self->got_ip_.store(false, std::memory_order_release);
    }
  };
  // A dead peer or an LCP failure is reported here, not as a lost IP event; esp_modem enables these
  // error events itself when it attaches the netif
  auto ppp_status = [](void *arg, esp_event_base_t, int32_t event_id, void *) {
    if (event_id < NETIF_PP_PHASE_OFFSET && event_id != NETIF_PPP_ERRORNONE) {
      ESP_LOGD(TAG, "PPP error event %d", static_cast<int>(event_id));
      static_cast<ModemComponent *>(arg)->got_ip_.store(false, std::memory_order_release);
    }
  };
  if (esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event, this) != ESP_OK ||
      esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, ppp_status, this) != ESP_OK) {
    this->fail_setup_(LOG_STR("Event handler register failed"));
    return;
  }

  // The DTE and DCE live for the life of the program; a baud probe only changes the UART divisor
  esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
  dte_config.uart_config.tx_io_num = this->tx_pin_->get_pin();
  dte_config.uart_config.rx_io_num = this->rx_pin_->get_pin();
  dte_config.uart_config.rx_buffer_size = this->rx_buffer_size_;
  dte_config.uart_config.tx_buffer_size = this->tx_buffer_size_;
  dte_config.uart_config.event_queue_size = UART_EVENT_QUEUE_SIZE;
  dte_config.uart_config.port_num = this->uart_port_;
  if (this->baud_rate_ != 0) {
    dte_config.uart_config.baud_rate = this->baud_rate_;
  }
  if (this->rts_pin_ != nullptr) {
    dte_config.uart_config.rts_io_num = this->rts_pin_->get_pin();
    dte_config.uart_config.cts_io_num = this->cts_pin_->get_pin();
    dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_HW;
  }
  dte_config.dte_buffer_size = this->dte_buffer_size_;
  this->current_baud_rate_ = dte_config.uart_config.baud_rate;
  this->dte_ = create_uart_dte(&dte_config);
  esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(this->apn_);
  if (this->dte_) {
    this->dce_ = this->dce_factory_(&dce_config, this->dte_, this->ppp_netif_);
  }
  if (!this->dce_) {
    this->fail_setup_(LOG_STR("Modem DTE/DCE creation failed"));
    return;
  }
#ifdef USE_MODEM_URC
  // Runs on the UART task: keep one line in a fixed buffer and let loop() publish it
  this->dce_->set_enhanced_urc([this](const DTE::UrcBufferInfo &buffer_info) {
    const DTE::UrcConsumeInfo none{DTE::UrcConsumeResult::CONSUME_NONE, 0};
    // The response parser owns the buffer while a command runs; URCs in that window are best effort
    if (buffer_info.is_command_active) {
      return none;
    }
    // The chunk is raw UART data; drop the line ends around the URC
    const uint8_t *start = buffer_info.new_data_start;
    size_t len = buffer_info.new_data_size;
    while (len > 0 && (*start == '\r' || *start == '\n')) {
      start++;
      len--;
    }
    while (len > 0 && (start[len - 1] == '\r' || start[len - 1] == '\n')) {
      len--;
    }
    if (len == 0) {
      return none;
    }
    if (this->urc_pending_.load(std::memory_order_acquire)) {
      ESP_LOGW(TAG, "URC dropped; the previous one is not published yet");
      return none;
    }
    len = std::min(len, sizeof(this->urc_line_) - 1);
    memcpy(this->urc_line_, start, len);
    this->urc_line_[len] = '\0';
    this->urc_pending_.store(true, std::memory_order_release);
    this->defer([this]() {
      this->urc_text_sensor_->publish_state(this->urc_line_);
      this->urc_pending_.store(false, std::memory_order_release);
    });
    return none;
  });
#endif
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
      this->tx_pin_->get_pin(), this->rx_pin_->get_pin(), this->uart_port_, this->current_baud_rate_,
      this->tx_buffer_size_, this->rx_buffer_size_, this->dte_buffer_size_, this->timeout_, YESNO(this->is_enabled()));
  this->dump_connect_params_();
}

void ModemComponent::loop() {
  // Some modem steps need a pause; the early return alone defers the work
  if (App.get_loop_component_start_time() - this->delay_start_ < this->delay_ms_) {
    return;
  }
  this->transition_to_(this->compute_next_state_());
}

ModemComponentState ModemComponent::retry_(ModemComponentState state, uint32_t delay_ms) {
  this->delay_start_ = App.get_loop_component_start_time();
  this->delay_ms_ = delay_ms;
  return state;
}

ModemComponentState ModemComponent::compute_next_state_() {
  if (this->target_state_ == ModemComponentState::MODEM_DISABLED) {
    if (this->component_state_ == ModemComponentState::MODEM_DISABLED) {
      this->disable_loop();  // Parked until enable() turns the loop back on
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
      this->init_at_failed_ = false;
      for (uint8_t i = 0; i != this->init_at_count_; i++) {
        App.feed_wdt();
        if (!this->send_at(this->init_at_commands_[i], 0, true)) {
          this->init_at_failed_ = true;
          this->status_set_warning(LOG_STR("init_at command failed"));
        }
      }
      this->connect_step_ = 0;
      return ModemComponentState::MODEM_CONNECTING;
    case ModemComponentState::MODEM_CONNECTING:
      return this->handle_state_connecting_();
    case ModemComponentState::MODEM_WAIT_IP:
      return this->handle_state_wait_ip_();
    case ModemComponentState::MODEM_CONNECTED:
      if (this->got_ip_.load(std::memory_order_acquire)) {
        return ModemComponentState::MODEM_CONNECTED;
      }
      ESP_LOGW(TAG, "Lost IP");
      return ModemComponentState::MODEM_DISCONNECTING;
    case ModemComponentState::MODEM_DISCONNECTING:
      return this->handle_state_disconnecting_();
  }
  return this->component_state_;
}

ModemComponentState ModemComponent::handle_state_enabling_() {
  // Find the baud rate the modem answers at and leave it in command mode, synced
  this->dce_->set_mode(modem_mode::AUTODETECT);
  auto mode = this->dce_->get_mode();
  ESP_LOGD(TAG, "Autodetect mode: %d", static_cast<int>(mode));

  switch (mode) {
    case modem_mode::CMUX_MANUAL_MODE:
      ESP_LOGD(TAG, "Modem warm reboot. Exiting CMUX");
      if (!this->dce_->set_mode(modem_mode::CMUX_MANUAL_EXIT)) {
        ESP_LOGW(TAG, "Failed to exit CMUX");
      }
      break;

    case modem_mode::UNDEF: {
      // One candidate per pass keeps each loop() pass short; the current baud was just probed
      const int bauds[] = {this->restored_baud_, this->baud_rate_, FALLBACK_BAUD_RATE};
      const int baud = bauds[this->baud_index_++ % 3];
      if (baud == 0) {
        return ModemComponentState::MODEM_ENABLING;
      }
      if (uart_set_baudrate(this->uart_port_, baud) != ESP_OK) {
        ESP_LOGW(TAG, "UART rejected baud rate %d", baud);
        return this->retry_(ModemComponentState::MODEM_ENABLING);
      }
      this->current_baud_rate_ = baud;
      App.feed_wdt();
      if (!this->send_at("AT", 100)) {
        ESP_LOGW(TAG, "Modem not responding at %d baud. Check power, PWRKEY and TX/RX wiring.", baud);
        return this->retry_(ModemComponentState::MODEM_ENABLING);
      }
      ESP_LOGD(TAG, "Modem responded at baud %d", baud);
      break;
    }

    case modem_mode::COMMAND_MODE:
      break;

    case modem_mode::DATA_MODE:
      // Can block 20 s; esp_modem offers no shorter exit
      ESP_LOGW(TAG, "Exiting DATA_MODE (long blocking call, make sure to have a long WDT)");
      if (!this->dce_->set_mode(modem_mode::COMMAND_MODE)) {
        ESP_LOGW(TAG, "Failed to leave data mode");
      }
      break;

    default:
      ESP_LOGW(TAG, "Modem in unexpected mode %d. Switching to COMMAND mode.", static_cast<int>(mode));
      if (!this->dce_->set_mode(modem_mode::COMMAND_MODE)) {
        ESP_LOGW(TAG, "Failed to switch to command mode");
      }
      this->dce_->set_mode(modem_mode::UNDEF);
      break;
  }

  App.feed_wdt();  // a mode switch can take up to 20 s; the watchdog floor covers one long call
  if (this->dce_->sync() != command_result::OK) {
    ESP_LOGW(TAG, "Waiting SYNC");
    return this->retry_(ModemComponentState::MODEM_ENABLING);
  }

  int radio_state = 0;
  if (this->dce_->get_radio_state(radio_state) != command_result::OK) {
    ESP_LOGW(TAG, "Failed to read the radio state; skipping the radio off step");
  } else if (radio_state == 1) {
    // On warm reboot, drop the previous network session first; a modem that refuses is used as it is
    if (this->dce_->set_radio_state(0) == command_result::OK) {
      return this->retry_(ModemComponentState::MODEM_ENABLING);
    }
    ESP_LOGW(TAG, "Failed to disconnect previous network session");
  }

  if (this->baud_rate_ != 0 && this->baud_rate_ != this->current_baud_rate_) {
    ESP_LOGD(TAG, "Setting baud rate: %d -> %d", this->current_baud_rate_, this->baud_rate_);
    if (this->dce_->set_baud(this->baud_rate_) == command_result::OK) {
      delay(200);  // NOLINT
      if (uart_set_baudrate(this->uart_port_, this->baud_rate_) != ESP_OK) {
        ESP_LOGE(TAG, "UART rejected baud rate %d", this->baud_rate_);
        return ModemComponentState::MODEM_DISCONNECTING;
      }
      this->current_baud_rate_ = this->baud_rate_;
      if (this->dce_->sync() != command_result::OK) {
        ESP_LOGE(TAG, "Failed to sync modem at baud rate %d.", this->baud_rate_);
        this->status_set_warning(LOG_STR("No answer at the configured baud rate"));
        return ModemComponentState::MODEM_DISCONNECTING;
      }
      ESP_LOGI(TAG, "Modem synced at baud rate %d.", this->current_baud_rate_);
    } else {
      // The modem still answers at the probed rate, so a rejected rate is not worth a retry loop
      ESP_LOGW(TAG, "Modem rejected baud rate %d. Staying at %d.", this->baud_rate_, this->current_baud_rate_);
    }
  }

  if (this->restored_baud_ != this->current_baud_rate_) {
    this->restored_baud_ = this->current_baud_rate_;
    this->pref_.save(&this->restored_baud_);
  }
  return ModemComponentState::MODEM_SYNCED;
}

ModemComponentState ModemComponent::handle_state_connecting_() {
  // The long commands run once; the retry passes only poll the attachment
  if (this->connect_step_ < 1) {
    if (this->dce_->sync() != command_result::OK) {
      ESP_LOGW(TAG, "Modem not synced during network init");
      return ModemComponentState::MODEM_ENABLING;
    }
    if (this->dce_->set_radio_state(1) != command_result::OK) {
      ESP_LOGW(TAG, "Failed to turn the radio on");
      return this->retry_(ModemComponentState::MODEM_CONNECTING);
    }
    this->connect_step_ = 1;
  }

  if (this->connect_step_ < 2) {
    switch (this->prepare_sim_()) {
      case SimResult::SIM_RESULT_READY:
        break;
      case SimResult::SIM_RESULT_RETRY:
        return this->retry_(ModemComponentState::MODEM_CONNECTING);
      case SimResult::SIM_RESULT_FATAL:
        // Retrying would burn the remaining PIN attempts; stay off until the user fixes the SIM
        this->status_set_error(LOG_STR("SIM locked"));
        this->target_state_ = ModemComponentState::MODEM_DISABLED;
        this->cancel_timeout(CONNECT_TIMEOUT_ID);
        return ModemComponentState::MODEM_DISCONNECTING;
    }
    if (this->dce_->set_network_attachment_state(1) != command_result::OK) {
      ESP_LOGW(TAG, "Failed to request network attachment");
      return this->retry_(ModemComponentState::MODEM_CONNECTING);
    }
    this->connect_step_ = 2;
  }

  int attached = 0;
  if (this->dce_->get_network_attachment_state(attached) != command_result::OK) {
    ESP_LOGW(TAG, "Failed to read the network attachment state");
    this->connect_step_ = 0;  // start over with a sync on the next pass
    return this->retry_(ModemComponentState::MODEM_CONNECTING);
  }
  if (!attached) {
    int rssi = 99, ber = 99, registration = -1, mode = -1;  // -1 stays when the query fails
    this->dce_->get_network_registration_state(registration);
    this->dce_->get_signal_quality(rssi, ber);
    this->dce_->get_network_system_mode(mode);
    ESP_LOGW(TAG, "Modem not attached yet: registration %d, network type %d, CSQ %d, BER %d", registration, mode, rssi,
             ber);
    return this->retry_(ModemComponentState::MODEM_CONNECTING, 4 * RETRY_DELAY_MS);
  }

  ESP_LOGI(TAG, "Modem initialized and ready, starting PPP");
  if (!this->dce_->set_mode(modem_mode::CMUX_MODE)) {
    ESP_LOGE(TAG, "Failed to enter PPP. Resetting modem.");
    return ModemComponentState::MODEM_DISCONNECTING;
  }
  this->wait_ip_start_ = App.get_loop_component_start_time();
  return ModemComponentState::MODEM_WAIT_IP;
}

ModemComponentState ModemComponent::handle_state_wait_ip_() {
  if (this->got_ip_.load(std::memory_order_acquire)) {
    this->cancel_timeout(CONNECT_TIMEOUT_ID);
    if (!this->init_at_failed_) {
      this->status_clear_warning();
    }
    this->status_clear_error();  // a SIM fault fixed outside is over once PPP is up
    this->dump_connect_params_();
    return ModemComponentState::MODEM_CONNECTED;
  }
  if (App.get_loop_component_start_time() - this->wait_ip_start_ < WAIT_IP_TIMEOUT_MS) {
    return ModemComponentState::MODEM_WAIT_IP;
  }
  ESP_LOGE(TAG, "Unable to get IP address");
  return ModemComponentState::MODEM_DISCONNECTING;
}

ModemComponentState ModemComponent::handle_state_disconnecting_() {
  // COMMAND_MODE stops PPP and leaves CMUX; the manual exit only applies to a warm reboot
  bool clean = true;
  if (this->dce_->get_mode() != modem_mode::COMMAND_MODE && !this->dce_->set_mode(modem_mode::COMMAND_MODE)) {
    // The next enable() probes the mode again and retries the exit; the warning keeps this visible
    this->status_set_warning(LOG_STR("Failed to leave data mode"));
    clean = false;
  }
  App.feed_wdt();
  // Turn the radio off; waiting for the lost IP event could take up to 2 min
  if (this->dce_->set_radio_state(0) == command_result::OK) {
    ESP_LOGI(TAG, "Radio off");
  } else {
    this->status_set_warning(LOG_STR("Radio off failed"));
    clean = false;
  }
  if (clean && !this->init_at_failed_) {
    this->status_clear_warning();  // a clean teardown ends whatever the warning was about
  }
  return ModemComponentState::MODEM_DISABLED;
}

void ModemComponent::transition_to_(ModemComponentState next_state) {
  if (next_state == this->component_state_) {
    return;
  }
  const bool lost = next_state == ModemComponentState::MODEM_DISCONNECTING &&
                    this->component_state_ == ModemComponentState::MODEM_CONNECTED;
  ESP_LOGD(TAG, "State change: %d -> %d", static_cast<int>(this->component_state_), static_cast<int>(next_state));
  this->component_state_ = next_state;
  // on_disconnect pairs with on_connect, so a failed attempt that never connected does not fire it
  if (next_state != ModemComponentState::MODEM_DISCONNECTING || lost) {
    this->on_state_callback_.call(next_state);
  }
  if (next_state == ModemComponentState::MODEM_DISCONNECTING) {
    // The lost IP event can lag by minutes; a quick re-enable must wait for a fresh address
    this->got_ip_.store(false, std::memory_order_release);
    this->retry_(next_state, COMMAND_TIMEOUT_MS);  // let the link settle before the first pass
    if (lost && this->target_state_ == ModemComponentState::MODEM_CONNECTED) {
      this->arm_connect_timeout_();  // A lost connection starts the reconnect watchdog again
    }
  }
}

bool ModemComponent::send_at(const std::string &cmd, uint32_t timeout, bool verbose) {
  if (!this->dce_) {
    ESP_LOGE(TAG, "Modem DCE not ready, cannot send AT command: %s", cmd.c_str());
    return false;
  }
  std::string output;
  const bool ok = this->dce_->at(cmd, output, timeout == 0 ? COMMAND_TIMEOUT_MS : timeout) == command_result::OK;
  if (!verbose) {
    return ok;
  }
  if (ok) {
    ESP_LOGI(TAG, "AT '%s' -> OK: %s", cmd.c_str(), output.c_str());
  } else {
    ESP_LOGW(TAG, "AT '%s' -> FAILED: %s", cmd.c_str(), output.c_str());
  }
  return ok;
}

SimResult ModemComponent::prepare_sim_() {
  std::string output;
  if (this->dce_->at("AT+CPIN?", output, COMMAND_TIMEOUT_MS) != command_result::OK) {
    ESP_LOGW(TAG, "Failed to check pin: %s", output.c_str());
    return SimResult::SIM_RESULT_RETRY;
  }
  const char *status = output.c_str();
  if (output.starts_with("+CPIN: ")) {
    status += 7;
  }
  ESP_LOGD(TAG, "SIM: %s", status);

  // A whole-token match: "NOT READY" also contains READY
  if (strncmp(status, "READY", 5) == 0) {
    this->pin_sent_ = false;  // proven correct, so a modem self-reset may send it again
    return SimResult::SIM_RESULT_READY;
  }
  if (strstr(status, "PUK") != nullptr) {
    ESP_LOGE(TAG, "SIM is PUK locked; unlock it in a phone");
    return SimResult::SIM_RESULT_FATAL;
  }
  if (strstr(status, "PIN") == nullptr) {
    ESP_LOGW(TAG, "SIM not ready: %s", status);  // e.g. NOT READY or NOT INSERTED
    return SimResult::SIM_RESULT_RETRY;
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
  const command_result result = this->dce_->set_pin(this->pin_code_);
  if (result == command_result::TIMEOUT) {
    ESP_LOGW(TAG, "No answer to the PIN; AT+CPIN? decides on the next pass");
  } else if (result != command_result::OK) {
    ESP_LOGE(TAG, "The modem rejected the PIN; check pin_code");
    return SimResult::SIM_RESULT_FATAL;
  }
  return SimResult::SIM_RESULT_RETRY;  // AT+CPIN? confirms READY on the next pass
}

void ModemComponent::dump_connect_params_() {
  esp_netif_ip_info_t ip;
  if (!this->got_ip_.load(std::memory_order_acquire) || esp_netif_get_ip_info(this->ppp_netif_, &ip) != ESP_OK) {
    ESP_LOGCONFIG(TAG, "  Connection: not connected");
    return;
  }
  auto dns_str = [this](esp_netif_dns_type_t type, char *buf) -> const char * {
    esp_netif_dns_info_t dns{};
    if (esp_netif_get_dns_info(this->ppp_netif_, type, &dns) != ESP_OK) {
      return LOG_STR_LITERAL("unavailable");
    }
    return network::IPAddress(&dns.ip.u_addr.ip4).str_to(buf);
  };
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
                dns_str(ESP_NETIF_DNS_MAIN, dns_main_buf), dns_str(ESP_NETIF_DNS_BACKUP, dns_backup_buf));
}

}  // namespace esphome::modem

#endif
