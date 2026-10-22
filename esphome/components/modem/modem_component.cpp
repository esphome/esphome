#ifdef USE_ESP32
#include "modem_component.h"
#include "soc/soc_caps.h"
#include <driver/uart.h>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/components/network/util.h"

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_netif_ppp.h>

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <iterator>

namespace esphome::modem {

using esp_modem::command_result;
using esp_modem::create_uart_dte;
using esp_modem::DTE;
using esp_modem::modem_mode;

ESPHOME_LOG_TAG(TAG, "modem");

#ifdef SOC_UART_HP_NUM
static constexpr int HP_UART_COUNT = SOC_UART_HP_NUM;
#else
static constexpr int HP_UART_COUNT = UART_NUM_MAX;
#endif

ModemComponent *global_modem_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static constexpr uint32_t WAIT_IP_TIMEOUT_MS = 50000;
static constexpr uint32_t PROBE_TIMEOUT_MS = 100;
// The attachment is polled every 4 s; the signal and operator queries only every 4th poll
static constexpr uint32_t ATTACH_POLL_MS = 4000;
static constexpr uint8_t ATTACH_DIAG_EVERY = 4;
static constexpr uint8_t UART_EVENT_QUEUE_SIZE = 10;
static constexpr uint32_t BAUD_PREF_HASH = 76007670UL;
static constexpr uint32_t PIN_PREF_HASH = 76007671UL;
static constexpr int FALLBACK_BAUD_RATE = 115200;

// Indexed by ModemState
PROGMEM_STRING_TABLE(ModemStateStrings, "DISABLED", "ENABLING", "SYNCED", "CONNECTING", "WAIT_IP", "CONNECTED",
                     "DISCONNECTING", "UNKNOWN");
static const LogString *state_to_str(ModemState state) {
  return ModemStateStrings::get_log_str(static_cast<uint8_t>(state), ModemStateStrings::LAST_INDEX);
}

ModemComponent::ModemComponent(InternalGPIOPin *tx_pin, InternalGPIOPin *rx_pin, const char *model, DceFactory factory,
                               const char *apn)
    : tx_pin_(tx_pin), rx_pin_(rx_pin), model_(model), dce_factory_(factory), apn_(apn) {
  global_modem_component = this;
}

void ModemComponent::enable() {
  ESP_LOGI(TAG, "Setting modem target state to CONNECTED");
  this->target_state_.store(ModemState::MODEM_STATE_CONNECTED);
  // A link that is already up has nothing to wait for; a lost one re-arms the timer itself
  if (this->component_state_.load() != ModemState::MODEM_STATE_CONNECTED) {
    this->arm_connect_timeout_();
  }
  this->wake_task_();
}

void ModemComponent::disable() {
  ESP_LOGI(TAG, "Setting modem target state to DISABLED");
  this->target_state_.store(ModemState::MODEM_STATE_DISABLED);
  this->cancel_connect_timeout_();
  this->wake_task_();
}

void ModemComponent::wake_task_() {
  if (this->task_.is_created()) {
    xTaskNotifyGive(this->task_.get_handle());
  }
}

// Self-keyed so it still fires after a failed setup, where the component's own timers are skipped
void ModemComponent::arm_connect_timeout_() {
  // Measures time without a connection, like wifi: neither retry cycles nor a repeated enable() restart it
  if (this->timeout_ == 0 || this->connect_timeout_armed_) {
    return;
  }
  this->connect_timeout_armed_ = true;
  App.scheduler.set_timeout(this, this->timeout_, [this]() {
    // The task may have connected between enable() checking the state and arming this timer
    this->connect_timeout_armed_ = false;
    if (this->is_connected()) {
      return;
    }
    ESP_LOGE(TAG, "Modem was not able to connect (timeout), rebooting");
    App.reboot();
  });
}

void ModemComponent::cancel_connect_timeout_() {
  this->connect_timeout_armed_ = false;
  App.scheduler.cancel_timeout(this);
}

network::IPAddresses ModemComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  esp_netif_ip_info_t ip;
  if (this->is_connected() && esp_netif_get_ip_info(this->ppp_netif_, &ip) == ESP_OK) {
    addresses[0] = network::IPAddress(&ip.ip);
#if USE_NETWORK_IPV6
    network::fill_ip6_addresses(this->ppp_netif_, addresses);
#endif /* USE_NETWORK_IPV6 */
  }
  return addresses;
}

void ModemComponent::fail_setup_(const LogString *reason) {
  this->mark_failed(reason);
  if (this->target_state_.load() == ModemState::MODEM_STATE_CONNECTED) {
    this->arm_connect_timeout_();
  }
}

// The status flags, the automations and the preferences belong to the main loop, so the task leaves the
// work in pending_ and wakes loop()
void ModemComponent::hand_over_(uint32_t pending) {
  this->pending_.fetch_or(pending, std::memory_order_release);
  this->enable_loop_soon_any_context();
}

void ModemComponent::set_warning_(uint8_t bit) {
  // The task is the only writer, so a load and a store do; a byte-sized read-modify-write is a library call on Xtensa
  const uint8_t warnings = this->warnings_.load(std::memory_order_relaxed);
  this->warnings_.store(warnings | bit, std::memory_order_relaxed);
  if ((warnings & bit) == 0)
    this->hand_over_(PENDING_STATUS);
}

void ModemComponent::clear_warning_(uint8_t bit) {
  const uint8_t warnings = this->warnings_.load(std::memory_order_relaxed);
  this->warnings_.store(warnings & static_cast<uint8_t>(~bit), std::memory_order_relaxed);
  if ((warnings & bit) != 0)
    this->hand_over_(PENDING_STATUS);
}

void ModemComponent::set_sim_fault_(SimFault fault) {
  const SimFault previous = this->sim_fault_.load(std::memory_order_relaxed);
  this->sim_fault_.store(fault, std::memory_order_relaxed);
  if (previous != fault)
    this->hand_over_(PENDING_STATUS);
}

void ModemComponent::publish_status_() {
  const uint8_t warnings = this->warnings_.load(std::memory_order_relaxed);
  if (warnings == 0) {
    this->status_clear_warning();
  } else if (warnings & WARN_BAUD) {
    this->status_set_warning(LOG_STR("Configured baud rate failed"));  // a local cause before a silent modem
  } else if (warnings & WARN_NO_RESPONSE) {
    this->status_set_warning(LOG_STR("Modem not responding"));
  } else if (warnings & WARN_SIM) {
    this->status_set_warning(LOG_STR("SIM not ready"));
  } else if (warnings & WARN_SETUP) {
    this->status_set_warning(LOG_STR("APN or radio setup failed"));
  } else if (warnings & WARN_NOT_ATTACHED) {
    this->status_set_warning(LOG_STR("No network attachment"));
  } else if (warnings & WARN_PPP) {
    this->status_set_warning(LOG_STR("PPP link failed"));
  } else if (warnings & WARN_TEARDOWN) {
    this->status_set_warning(LOG_STR("Modem teardown failed"));
  } else {
    this->status_set_warning(LOG_STR("init_at command failed"));
  }
  const SimFault fault = this->sim_fault_.load(std::memory_order_relaxed);
  if (fault == SimFault::SIM_FAULT_PIN_REJECTED) {
    this->status_set_error(LOG_STR("SIM PIN rejected; change pin_code"));
  } else if (fault == SimFault::SIM_FAULT_PIN_UNCONFIRMED) {
    this->status_set_error(LOG_STR("SIM still locked after PIN; check SIM"));
  } else if (fault == SimFault::SIM_FAULT_LOCKED) {
    this->status_set_error(LOG_STR("SIM locked"));
  } else {
    this->status_clear_error();
  }
}

void ModemComponent::loop() {
  const uint32_t pending = this->pending_.exchange(0, std::memory_order_acquire);
  // In order, so each automation sees every state the task went through
  uint8_t tail = this->state_tail_.load(std::memory_order_relaxed);
  while (tail != this->state_head_.load(std::memory_order_acquire)) {
    const ModemState state = this->state_queue_[tail % STATE_QUEUE_SIZE];
    if (state == ModemState::MODEM_STATE_CONNECTED) {
      this->dump_connect_params_();
    }
    this->on_state_callback_.call(state);
    // Freed after the automations ran, so the task's SYNCED gate sees on_sync finished, not just started
    this->state_tail_.store(++tail, std::memory_order_release);
  }
  if (pending & PENDING_STATUS) {
    this->publish_status_();
  }
  if (pending & PENDING_CONNECT_TIMEOUT) {
    if (this->target_state_.load() == ModemState::MODEM_STATE_CONNECTED && !this->is_connected()) {
      this->arm_connect_timeout_();
    } else {
      this->cancel_connect_timeout_();
    }
  }
  if ((pending & PENDING_SAVE_BAUD) && !this->pref_.save(&this->restored_baud_)) {
    ESP_LOGW(TAG, "Failed to save the baud rate");
  }
  // Written through at once: a power cycle before the periodic flush would send the PIN again
  if (pending & PENDING_SAVE_PIN) {
    // The task leaves pin_record_to_save_ alone until pin_save_state_ reports the outcome
    if (this->pin_pref_.save(&this->pin_record_to_save_) && global_preferences->sync()) {
      this->pin_save_retries_ = 0;
      this->pin_save_state_.store(1, std::memory_order_release);
      this->wake_task_();
    } else if (this->pin_save_retries_ < PIN_SAVE_RETRIES) {
      this->pin_save_retries_++;
      ESP_LOGW(TAG, "Failed to save the PIN record; retrying");
      this->set_timeout(PIN_SAVE_RETRY_MS, [this]() { this->hand_over_(PENDING_SAVE_PIN); });
    } else {
      ESP_LOGE(TAG, "Failed to save the PIN record");
      this->pin_save_state_.store(2, std::memory_order_release);
      this->wake_task_();
    }
  }
#ifdef USE_MODEM_URC
  if (this->urc_pending_.load(std::memory_order_acquire)) {
    this->urc_text_sensor_->publish_state(this->urc_buf_);
    this->urc_pending_.store(0, std::memory_order_release);
  }
#endif
  // A hand-over after these checks re-enables the loop through enable_loop_soon_any_context()
  if (this->pending_.load(std::memory_order_acquire) == 0 &&
      this->state_head_.load(std::memory_order_acquire) == this->state_tail_.load(std::memory_order_relaxed)) {
    this->disable_loop();
  }
}

void ModemComponent::setup() {
  this->pref_ = global_preferences->make_preference<int>(BAUD_PREF_HASH);
  this->pref_.load(&this->restored_baud_);
  this->pin_pref_ = global_preferences->make_preference<PinRecord>(PIN_PREF_HASH);
  this->pin_pref_.load(&this->pin_record_);
  // A changed pin_code starts with a clean record and may be tried again
  const uint32_t pin_hash = this->pin_code_ != nullptr ? fnv1_hash(this->pin_code_) : 0;
  if (this->pin_record_.hash != pin_hash) {
    this->pin_record_ = PinRecord{pin_hash, 0, false};
    if (!this->pin_pref_.save(&this->pin_record_)) {
      ESP_LOGW(TAG, "Failed to clear the PIN record");
    }
  }

  // Highest free high-performance port first; UART0 stays with the logger and the low-power UART of the
  // C6/P4 has restricted pins and clocks
  for (int p = HP_UART_COUNT - 1; p > UART_NUM_0; p--) {
    if (!uart_is_driver_installed(static_cast<uart_port_t>(p))) {
      this->uart_port_ = p;
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
    const bool got_ip = event_id == IP_EVENT_PPP_GOT_IP;
    if (!got_ip && event_id != IP_EVENT_PPP_LOST_IP) {
      return;
    }
    if (got_ip) {
      ESP_LOGD(TAG, "PPP IP event: Got IP " IPSTR, IP2STR(&static_cast<ip_event_got_ip_t *>(event_data)->ip_info.ip));
    } else {
      ESP_LOGD(TAG, "PPP IP event: Lost IP");
    }
    auto *self = static_cast<ModemComponent *>(arg);
    self->got_ip_.store(got_ip, std::memory_order_release);
    self->wake_task_();
  };
  // A dead peer or an LCP failure is reported here, not as a lost IP event; esp_modem enables these
  // error events itself when it attaches the netif
  auto ppp_status = [](void *arg, esp_event_base_t, int32_t event_id, void *) {
    if (event_id < NETIF_PP_PHASE_OFFSET && event_id != NETIF_PPP_ERRORNONE) {
      ESP_LOGW(TAG, "PPP error %d", static_cast<int>(event_id));
      auto *self = static_cast<ModemComponent *>(arg);
      self->ppp_error_.store(static_cast<uint8_t>(event_id), std::memory_order_release);
      self->got_ip_.store(0, std::memory_order_release);
      self->wake_task_();
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
  dte_config.uart_config.port_num = static_cast<uart_port_t>(this->uart_port_);
  // The rate the modem answered at last time comes first: after a baud_rate change the modem may still sit in
  // CMUX at the old rate, which only that rate can detect; the configured rate is applied once synced
  const int start_baud = this->restored_baud_ != 0 ? this->restored_baud_ : this->baud_rate_;
  if (start_baud != 0) {
    dte_config.uart_config.baud_rate = start_baud;
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
  // The pin schema's inverted flag is applied to the UART signals, as the uart component does
  uint32_t invert = 0;
  if (this->tx_pin_->is_inverted()) {
    invert |= UART_SIGNAL_TXD_INV;
  }
  if (this->rx_pin_->is_inverted()) {
    invert |= UART_SIGNAL_RXD_INV;
  }
  if (this->rts_pin_ != nullptr) {
    if (this->rts_pin_->is_inverted()) {
      invert |= UART_SIGNAL_RTS_INV;
    }
    if (this->cts_pin_->is_inverted()) {
      invert |= UART_SIGNAL_CTS_INV;
    }
  }
  if (invert != 0 && uart_set_line_inverse(static_cast<uart_port_t>(this->uart_port_), invert) != ESP_OK) {
    this->fail_setup_(LOG_STR("UART line inversion failed"));
    return;
  }
#ifdef USE_MODEM_URC
  // Runs on the UART task. CMUX hands each frame over on its own, so lines are assembled here; every
  // chunk is consumed and a complete line is handed to loop() in urc_buf_.
  this->dce_->set_enhanced_urc([this](const DTE::UrcBufferInfo &buffer_info) {
    // The response parser owns the buffer while a command runs; URCs in that window are best effort,
    // and a line started before it would otherwise be glued to the next one
    if (buffer_info.is_command_active) {
      this->urc_len_ = 0;
      this->urc_discard_ = false;
      return DTE::UrcConsumeInfo{DTE::UrcConsumeResult::CONSUME_NONE, 0};
    }
    for (size_t i = 0; i < buffer_info.new_data_size; i++) {
      const char c = static_cast<char>(buffer_info.new_data_start[i]);
      if (c == '\r' || c == '\n') {
        if (!this->urc_discard_ && this->urc_len_ != 0) {
          this->urc_buf_[this->urc_len_] = '\0';
          this->urc_pending_.store(1, std::memory_order_release);
          this->enable_loop_soon_any_context();
        }
        this->urc_len_ = 0;
        this->urc_discard_ = false;
      } else if (this->urc_discard_) {
        continue;
      } else if (this->urc_pending_.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "URC dropped; the previous one is not published yet");
        this->urc_discard_ = true;
      } else if (this->urc_len_ < sizeof(this->urc_buf_) - 1) {
        this->urc_buf_[this->urc_len_++] = c;
      } else {
        ESP_LOGW(TAG, "URC longer than %u bytes dropped", static_cast<unsigned>(sizeof(this->urc_buf_) - 1));
        this->urc_discard_ = true;
      }
    }
    return DTE::UrcConsumeInfo{DTE::UrcConsumeResult::CONSUME_ALL, 0};
  });
#endif
  if (!this->task_.create(ModemComponent::task_entry, "modem", TASK_STACK_SIZE, this, 1, false)) {
    this->fail_setup_(LOG_STR("Task creation failed"));
  }
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
      this->tx_pin_->get_pin(), this->rx_pin_->get_pin(), static_cast<int>(this->uart_port_), this->current_baud_rate_,
      this->tx_buffer_size_, this->rx_buffer_size_, this->dte_buffer_size_, this->timeout_, YESNO(this->is_enabled()));
  this->dump_connect_params_();
}

void ModemComponent::task_entry(void *arg) { static_cast<ModemComponent *>(arg)->run_task_(); }

void ModemComponent::run_task_() {
  while (true) {
    // A command from send_at() goes out between state steps, never in the middle of a sequence
    if (this->at_pending_.load(std::memory_order_acquire)) {
      this->at_(this->at_command_, this->at_timeout_);
      this->at_pending_.store(0, std::memory_order_release);
    }
    this->step_delay_ms_ = 0;
    this->transition_to_(this->compute_next_state_());
    // Nothing to do while disabled or connected: enable(), disable() and the PPP events wake the task
    const bool parked = this->component_state_.load() == this->target_state_.load();
    ulTaskNotifyTake(pdTRUE, parked ? portMAX_DELAY : pdMS_TO_TICKS(this->step_delay_ms_));
  }
}

ModemState ModemComponent::retry_(ModemState state, uint32_t delay_ms) {
  this->step_delay_ms_ = delay_ms;
  return state;
}

ModemState ModemComponent::compute_next_state_() {
  const ModemState state = this->component_state_.load();
  if (this->target_state_.load() == ModemState::MODEM_STATE_DISABLED) {
    if (state == ModemState::MODEM_STATE_DISABLED) {
      return ModemState::MODEM_STATE_DISABLED;
    }
    if (state != ModemState::MODEM_STATE_DISCONNECTING) {
      return ModemState::MODEM_STATE_DISCONNECTING;
    }
  }

  switch (state) {
    case ModemState::MODEM_STATE_DISABLED:
      this->pin_sent_ = false;  // a new attempt may send the PIN once
      this->pin_send_armed_ = false;
      return ModemState::MODEM_STATE_ENABLING;
    case ModemState::MODEM_STATE_ENABLING:
      return this->handle_state_enabling_();
    case ModemState::MODEM_STATE_SYNCED:
      // on_sync runs on the main loop and may queue an AT command or disable the modem; it gets a bounded head
      // start, and a queued command goes out at the top of the next pass before anything attaches
      if ((this->state_head_.load(std::memory_order_acquire) != this->state_tail_.load(std::memory_order_acquire) ||
           this->at_pending_.load(std::memory_order_acquire)) &&
          millis() - this->synced_at_ < SYNC_ACK_TIMEOUT_MS) {
        return this->retry_(ModemState::MODEM_STATE_SYNCED, SYNC_ACK_POLL_MS);
      }
      this->clear_warning_(WARN_INIT_AT);
      for (uint8_t i = 0; i != this->init_at_count_; i++) {
        if (!this->at_(this->init_at_commands_[i])) {
          this->set_warning_(WARN_INIT_AT);
        }
      }
      this->connect_step_ = ConnectStep::CONNECT_STEP_START;
      return ModemState::MODEM_STATE_CONNECTING;
    case ModemState::MODEM_STATE_CONNECTING:
      return this->handle_state_connecting_();
    case ModemState::MODEM_STATE_WAIT_IP:
      return this->handle_state_wait_ip_();
    case ModemState::MODEM_STATE_CONNECTED:
      if (this->got_ip_.load(std::memory_order_acquire)) {
        return ModemState::MODEM_STATE_CONNECTED;
      }
      ESP_LOGW(TAG, "Lost IP");
      return ModemState::MODEM_STATE_DISCONNECTING;
    case ModemState::MODEM_STATE_DISCONNECTING:
      return this->handle_state_disconnecting_();
  }
  return state;
}

ModemState ModemComponent::handle_state_enabling_() {
  // Find the baud rate the modem answers at and leave it in command mode, synced
  if (!this->dce_->set_mode(modem_mode::AUTODETECT)) {
    // A failed detection keeps the previous mode, which would skip the baud rate search below
    this->dce_->set_mode(modem_mode::UNDEF);
  }
  auto mode = this->dce_->get_mode();
  ESP_LOGD(TAG, "Autodetect mode: %d", static_cast<int>(mode));

  switch (mode) {
    case modem_mode::CMUX_MANUAL_MODE:
      // A data session survived the ESP reboot; PPP stops before the CMUX exit or esp_modem will not
      // start it on the next connect. Without a session this waits up to 30 s for a PPP exit.
      ESP_LOGD(TAG, "Modem warm reboot. Leaving the data session and CMUX");
      if (!this->dce_->set_mode(modem_mode::CMUX_MANUAL_COMMAND)) {
        ESP_LOGW(TAG, "Failed to stop the previous data session");
      }
      if (!this->dce_->set_mode(modem_mode::CMUX_MANUAL_EXIT)) {
        ESP_LOGW(TAG, "Failed to exit CMUX");
      }
      break;

    case modem_mode::UNDEF: {
      // The detection above probed the current rate; the other known rates are tried in this pass
      const int detected = this->current_baud_rate_;
      const int bauds[] = {this->restored_baud_, this->baud_rate_, FALLBACK_BAUD_RATE};
      bool found = false;
      bool probed = false;
      bool rejected = false;
      for (size_t i = 0; i != std::size(bauds) && !found; i++) {
        const int baud = bauds[i];
        if (baud == 0 || baud == detected || std::find(bauds, bauds + i, baud) != bauds + i) {
          continue;
        }
        if (uart_set_baudrate(static_cast<uart_port_t>(this->uart_port_), baud) != ESP_OK) {
          ESP_LOGW(TAG, "UART rejected baud rate %d", baud);
          this->set_warning_(WARN_BAUD);
          rejected = true;
          continue;
        }
        this->current_baud_rate_ = baud;
        probed = true;
        std::string output;
        found = this->dce_->at("AT", output, PROBE_TIMEOUT_MS) == command_result::OK;
        if (!found) {
          this->not_responding_(baud);
        }
      }
      if (!found) {
        if (!probed) {
          this->not_responding_(detected);  // the detection at the current rate already failed
        }
        return this->retry_(ModemState::MODEM_STATE_ENABLING, PROBE_RETRY_DELAY_MS);
      }
      ESP_LOGD(TAG, "Modem responded at baud %d", this->current_baud_rate_);
      break;
    }

    case modem_mode::COMMAND_MODE:
      break;

    case modem_mode::DATA_MODE:
      // Can block 20 s; esp_modem offers no shorter exit
      ESP_LOGW(TAG, "Exiting DATA_MODE");
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

  if (this->dce_->sync() != command_result::OK) {
    ESP_LOGW(TAG, "Waiting SYNC");
    this->set_warning_(WARN_NO_RESPONSE);
    return this->retry_(ModemState::MODEM_STATE_ENABLING);
  }

  int radio_state = 0;
  if (this->dce_->get_radio_state(radio_state) != command_result::OK) {
    ESP_LOGW(TAG, "Failed to read the radio state; skipping the radio off step");
  } else if (radio_state == 1) {
    // On warm reboot, drop the previous network session first; a modem that refuses is used as it is
    if (this->dce_->set_radio_state(0) == command_result::OK) {
      return this->retry_(ModemState::MODEM_STATE_ENABLING);
    }
    ESP_LOGW(TAG, "Failed to disconnect previous network session");
  }

  if (this->baud_rate_ != 0 && this->baud_rate_ != this->current_baud_rate_) {
    ESP_LOGD(TAG, "Setting baud rate: %d -> %d", this->current_baud_rate_, this->baud_rate_);
    if (this->dce_->set_baud(this->baud_rate_) == command_result::OK) {
      delay(200);  // NOLINT
      if (uart_set_baudrate(static_cast<uart_port_t>(this->uart_port_), this->baud_rate_) != ESP_OK) {
        ESP_LOGE(TAG, "UART rejected baud rate %d", this->baud_rate_);
        this->set_warning_(WARN_BAUD);
        return ModemState::MODEM_STATE_DISCONNECTING;
      }
      this->current_baud_rate_ = this->baud_rate_;
      if (this->dce_->sync() != command_result::OK) {
        ESP_LOGE(TAG, "Failed to sync modem at baud rate %d.", this->baud_rate_);
        this->set_warning_(WARN_BAUD);
        return ModemState::MODEM_STATE_DISCONNECTING;
      }
      ESP_LOGI(TAG, "Modem synced at baud rate %d.", this->current_baud_rate_);
    } else {
      // The modem still answers at the probed rate, so a rejected rate is not worth a retry loop
      ESP_LOGW(TAG, "Modem rejected baud rate %d. Staying at %d.", this->baud_rate_, this->current_baud_rate_);
      this->set_warning_(WARN_BAUD);
    }
  }

  if (this->restored_baud_ != this->current_baud_rate_) {
    this->restored_baud_ = this->current_baud_rate_;
    this->hand_over_(PENDING_SAVE_BAUD);
  }
  if (this->baud_rate_ == 0 || this->baud_rate_ == this->current_baud_rate_) {
    this->clear_warning_(WARN_BAUD);  // the rate in use is the configured one, or the default
  }
  this->clear_warning_(WARN_NO_RESPONSE);

  // The DTE side was set up with the UART; the modem side has to be asked for RTS/CTS as well
  if (this->rts_pin_ != nullptr && this->dce_->set_flow_control(2, 2) != command_result::OK) {
    ESP_LOGW(TAG, "Modem refused hardware flow control; UART overruns are possible");
  }
  // Numeric +CME ERROR codes tell a wrong PIN from other failures; without them any ERROR counts as a refusal
  this->cme_numeric_ = this->at_("AT+CMEE=1");
  this->modem_synced_ = true;
  this->synced_at_ = millis();
  return ModemState::MODEM_STATE_SYNCED;
}

ModemState ModemComponent::handle_state_connecting_() {
  if (this->connect_step_ == ConnectStep::CONNECT_STEP_START) {
    if (this->dce_->sync() != command_result::OK) {
      ESP_LOGW(TAG, "Modem not synced during network init");
      return ModemState::MODEM_STATE_ENABLING;
    }
    // The APN has to be in the PDP context before the attachment; esp_modem only sends it when PPP starts
    esp_modem::PdpContext pdp(this->apn_);
    if (this->dce_->set_pdp_context(pdp) != command_result::OK) {
      ESP_LOGW(TAG, "Failed to set the APN");
      this->set_warning_(WARN_SETUP);
      return this->retry_(ModemState::MODEM_STATE_CONNECTING);
    }
    if (this->dce_->set_radio_state(1) != command_result::OK) {
      ESP_LOGW(TAG, "Failed to turn the radio on");
      this->set_warning_(WARN_SETUP);
      return this->retry_(ModemState::MODEM_STATE_CONNECTING);
    }
    this->clear_warning_(WARN_SETUP);
    this->connect_step_ = ConnectStep::CONNECT_STEP_RADIO_ON;
    this->attach_polls_ = 0;
  }

  if (this->connect_step_ == ConnectStep::CONNECT_STEP_RADIO_ON) {
    const SimResult result = this->prepare_sim_();
    switch (result) {
      case SimResult::SIM_RESULT_READY:
        break;
      case SimResult::SIM_RESULT_RETRY:
        return this->retry_(ModemState::MODEM_STATE_CONNECTING);
      case SimResult::SIM_RESULT_FATAL:
      case SimResult::SIM_RESULT_PIN_REJECTED:
      case SimResult::SIM_RESULT_PIN_UNCONFIRMED:
      case SimResult::SIM_RESULT_PIN_GIVEN_UP:
        // Retrying would burn the remaining PIN attempts; stay off until the user fixes the SIM
        this->set_sim_fault_(result == SimResult::SIM_RESULT_FATAL          ? SimFault::SIM_FAULT_LOCKED
                             : result == SimResult::SIM_RESULT_PIN_REJECTED ? SimFault::SIM_FAULT_PIN_REJECTED
                                                                            : SimFault::SIM_FAULT_PIN_UNCONFIRMED);
        this->target_state_.store(ModemState::MODEM_STATE_DISABLED);
        // The first unconfirmed send keeps the reboot watchdog, since a reboot sends the PIN once more and may
        // get the SIM open; once that is used up a reboot would send nothing, so the watchdog is cancelled
        if (result != SimResult::SIM_RESULT_PIN_UNCONFIRMED) {
          this->hand_over_(PENDING_CONNECT_TIMEOUT);
        }
        return ModemState::MODEM_STATE_DISCONNECTING;
    }
    if (this->dce_->set_network_attachment_state(1) != command_result::OK) {
      ESP_LOGW(TAG, "Failed to request network attachment");
      this->set_warning_(WARN_NOT_ATTACHED);
      return this->retry_(ModemState::MODEM_STATE_CONNECTING);
    }
    this->connect_step_ = ConnectStep::CONNECT_STEP_ATTACH_REQUESTED;
  }

  int attached = 0;
  if (this->dce_->get_network_attachment_state(attached) != command_result::OK) {
    ESP_LOGW(TAG, "Failed to read the network attachment state");
    this->set_warning_(WARN_NOT_ATTACHED);
    this->connect_step_ = ConnectStep::CONNECT_STEP_START;  // start over with a sync on the next pass
    return this->retry_(ModemState::MODEM_STATE_CONNECTING);
  }
  if (!attached) {
    if (this->attach_polls_++ % ATTACH_DIAG_EVERY == 0) {
      int rssi = 99, ber = 99, registration = -1, mode = -1;  // -1 stays when the query fails
      this->dce_->get_network_registration_state(registration);
      this->dce_->get_signal_quality(rssi, ber);
      this->dce_->get_network_system_mode(mode);
      ESP_LOGW(TAG, "Modem not attached yet: registration %d, network type %d, CSQ %d, BER %d", registration, mode,
               rssi, ber);
    }
    this->set_warning_(WARN_NOT_ATTACHED);
    return this->retry_(ModemState::MODEM_STATE_CONNECTING, ATTACH_POLL_MS);
  }
  this->clear_warning_(WARN_NOT_ATTACHED);

  ESP_LOGI(TAG, "Modem initialized and ready, starting PPP");
  // A late event from the last session must not count
  this->got_ip_.store(0, std::memory_order_release);
  this->ppp_error_.store(0, std::memory_order_release);
  if (!this->dce_->set_mode(modem_mode::CMUX_MODE)) {
    ESP_LOGE(TAG, "Failed to enter PPP. Resetting modem.");
    this->set_warning_(WARN_PPP);
    return ModemState::MODEM_STATE_DISCONNECTING;
  }
  this->wait_ip_start_ = millis();
  return ModemState::MODEM_STATE_WAIT_IP;
}

ModemState ModemComponent::handle_state_wait_ip_() {
  if (this->got_ip_.load(std::memory_order_acquire)) {
    this->clear_warning_(WARN_TEARDOWN | WARN_PPP);
    this->set_sim_fault_(SimFault::SIM_FAULT_NONE);  // a SIM fault fixed outside is over once PPP is up
    return ModemState::MODEM_STATE_CONNECTED;
  }
  // A PPP failure (wrong APN, authentication, dead peer) ends the wait at once; the event logged the code
  if (this->ppp_error_.load(std::memory_order_acquire) != 0) {
    this->ppp_error_.store(0, std::memory_order_release);
    this->set_warning_(WARN_PPP);
    return ModemState::MODEM_STATE_DISCONNECTING;
  }
  const uint32_t elapsed = millis() - this->wait_ip_start_;
  if (elapsed < WAIT_IP_TIMEOUT_MS) {
    // The IP event wakes the task sooner
    return this->retry_(ModemState::MODEM_STATE_WAIT_IP, WAIT_IP_TIMEOUT_MS - elapsed);
  }
  ESP_LOGE(TAG, "Unable to get IP address");
  this->set_warning_(WARN_PPP);
  return ModemState::MODEM_STATE_DISCONNECTING;
}

ModemState ModemComponent::handle_state_disconnecting_() {
  // Each teardown command holds the task for up to 20 s, so a modem that never answered is skipped
  if (!this->modem_synced_) {
    return ModemState::MODEM_STATE_DISABLED;
  }
  this->modem_synced_ = false;
  // COMMAND_MODE stops PPP and leaves CMUX; the manual exit only applies to a warm reboot
  bool clean = true;
  if (this->dce_->get_mode() != modem_mode::COMMAND_MODE && !this->dce_->set_mode(modem_mode::COMMAND_MODE)) {
    // The next enable() probes the mode again and retries the exit; the warning keeps this visible
    ESP_LOGW(TAG, "Failed to leave data mode");
    this->set_warning_(WARN_TEARDOWN);
    clean = false;
  }
  // Turn the radio off; waiting for the lost IP event could take up to 2 min
  if (this->dce_->set_radio_state(0) == command_result::OK) {
    ESP_LOGI(TAG, "Radio off");
  } else {
    ESP_LOGW(TAG, "Radio off failed");
    this->set_warning_(WARN_TEARDOWN);
    clean = false;
  }
  if (clean) {
    this->clear_warning_(WARN_TEARDOWN);
  }
  return ModemState::MODEM_STATE_DISABLED;
}

void ModemComponent::transition_to_(ModemState next_state) {
  const ModemState state = this->component_state_.load();
  if (next_state == state) {
    return;
  }
  const bool lost = next_state == ModemState::MODEM_STATE_DISCONNECTING && state == ModemState::MODEM_STATE_CONNECTED;
  ESP_LOGD(TAG, "State change: %s -> %s", LOG_STR_ARG(state_to_str(state)), LOG_STR_ARG(state_to_str(next_state)));
  this->component_state_.store(next_state);
  if (next_state == ModemState::MODEM_STATE_CONNECTED) {
    this->hand_over_(PENDING_CONNECT_TIMEOUT);
  }
  // on_disconnect pairs with on_connect, so a failed attempt that never connected does not fire it; the
  // automations run on the main loop
  if (next_state != ModemState::MODEM_STATE_DISCONNECTING || lost) {
    const uint8_t head = this->state_head_.load(std::memory_order_relaxed);
    if (static_cast<uint8_t>(head - this->state_tail_.load(std::memory_order_acquire)) < STATE_QUEUE_SIZE) {
      this->state_queue_[head % STATE_QUEUE_SIZE] = next_state;
      this->state_head_.store(head + 1, std::memory_order_release);
      this->enable_loop_soon_any_context();
    } else {
      ESP_LOGW(TAG, "State change not handed to the automations; the main loop is behind");
    }
  }
  if (next_state == ModemState::MODEM_STATE_DISCONNECTING) {
    // The lost IP event can lag by minutes; a quick re-enable must wait for a fresh address
    this->got_ip_.store(0, std::memory_order_release);
    this->step_delay_ms_ = COMMAND_TIMEOUT_MS;  // let the link settle before the first pass
    if (lost) {
      this->hand_over_(PENDING_CONNECT_TIMEOUT);  // A lost connection starts the reconnect watchdog again
    }
  }
}

void ModemComponent::not_responding_(int baud) {
  ESP_LOGW(TAG, "Modem not responding at %d baud. Check power, PWRKEY and TX/RX wiring.", baud);
  this->set_warning_(WARN_NO_RESPONSE);
}

bool ModemComponent::send_at(const std::string &cmd, uint32_t timeout) { return this->send_at(cmd.c_str(), timeout); }

bool ModemComponent::send_at(const char *cmd, uint32_t timeout) {
  if (!this->task_.is_created()) {
    ESP_LOGW(TAG, "Modem not ready, cannot send AT command: %s", cmd);
    return false;
  }
  if (this->at_pending_.load(std::memory_order_acquire)) {
    ESP_LOGW(TAG, "AT command '%s' dropped; the previous one is not sent yet", cmd);
    return false;
  }
  const size_t len = strlen(cmd);
  if (len >= sizeof(this->at_command_)) {
    ESP_LOGW(TAG, "AT command longer than %u bytes dropped", static_cast<unsigned>(sizeof(this->at_command_) - 1));
    return false;
  }
  memcpy(this->at_command_, cmd, len + 1);
  this->at_timeout_ = timeout;
  this->at_pending_.store(1, std::memory_order_release);
  this->wake_task_();
  return true;
}

static bool line_is(const char *line, size_t len, const char *word) {
  const size_t n = strlen(word);
  return len == n && memcmp(line, word, n) == 0;
}

static bool line_starts_with(const char *line, size_t len, const char *prefix) {
  const size_t n = strlen(prefix);
  return len >= n && memcmp(line, prefix, n) == 0;
}

// A final result line of a reply: "OK", "ERROR", "+CME ERROR: n" or "+CMS ERROR: n", on a complete line of
// its own. An echoed command or a payload line that happens to contain OK is not one.
static command_result classify_reply(const std::string &reply) {
  size_t start = 0;
  while (start < reply.size()) {
    const size_t end = reply.find('\n', start);
    if (end == std::string::npos) {
      break;  // the last line is still incomplete
    }
    size_t stop = end;
    while (stop > start && (reply[stop - 1] == '\r' || reply[stop - 1] == '\n')) {
      stop--;
    }
    const char *line = reply.data() + start;
    const size_t len = stop - start;
    if (line_is(line, len, "OK")) {
      return command_result::OK;
    }
    if (line_is(line, len, "ERROR") || line_starts_with(line, len, "+CME ERROR") ||
        line_starts_with(line, len, "+CMS ERROR") || line_is(line, len, "NO CARRIER") || line_is(line, len, "BUSY") ||
        line_is(line, len, "NO DIALTONE") || line_is(line, len, "NO ANSWER")) {
      return command_result::FAIL;
    }
    start = end + 1;
  }
  return command_result::TIMEOUT;
}

command_result ModemComponent::command_raw_(const char *cmd, std::string &output, uint32_t timeout) {
  char line[AT_COMMAND_SIZE + 1];
  snprintf(line, sizeof(line), "%s\r", cmd);
  output.clear();
  const command_result result = this->dte_->command(
      line,
      [&output](uint8_t *data, size_t len) {
        // The UART terminal hands over the whole reply so far; a CMUX terminal hands over each frame on its own
        const char *chunk = reinterpret_cast<char *>(data);
        if (len >= output.size() && memcmp(chunk, output.data(), output.size()) == 0) {
          output.assign(chunk, len);
        } else {
          output.append(chunk, len);
        }
        if (output.size() > RAW_REPLY_MAX) {
          output.erase(0, output.size() - RAW_REPLY_MAX);  // the result line is at the end
        }
        return classify_reply(output);
      },
      timeout);
  // Trailing line ends only; a reply that spans lines is logged as it came
  while (!output.empty() && (output.back() == '\r' || output.back() == '\n')) {
    output.pop_back();
  }
  return result;
}

// Only the task changes pin_record_; loop() writes the copy, which stays untouched until it reports back
void ModemComponent::save_pin_record_() {
  this->pin_record_to_save_ = this->pin_record_;
  this->pin_save_state_.store(0, std::memory_order_release);
  this->hand_over_(PENDING_SAVE_PIN);
}

bool ModemComponent::at_(const char *cmd, uint32_t timeout) {
  std::string output;
  const command_result result = this->command_raw_(cmd, output, timeout);
  if (result == command_result::OK) {
    ESP_LOGI(TAG, "AT '%s' -> OK: %s", cmd, output.c_str());
  } else {
    ESP_LOGW(TAG, "AT '%s' -> %s: %s", cmd,
             result == command_result::TIMEOUT ? LOG_STR_LITERAL("TIMEOUT") : LOG_STR_LITERAL("FAILED"),
             output.c_str());
  }
  return result == command_result::OK;
}

SimResult ModemComponent::prepare_sim_() {
  std::string output;
  if (this->dce_->at("AT+CPIN?", output, COMMAND_TIMEOUT_MS) != command_result::OK) {
    ESP_LOGW(TAG, "Failed to check pin: %s", output.c_str());
    this->set_warning_(WARN_SIM);
    return SimResult::SIM_RESULT_RETRY;
  }
  const char *status = output.c_str();
  if (output.starts_with("+CPIN: ")) {
    status += 7;
  }
  ESP_LOGD(TAG, "SIM: %s", status);

  // A whole-token match: "NOT READY" also contains READY
  if (strncmp(status, "READY", 5) == 0) {
    this->clear_warning_(WARN_SIM);
    this->pin_sent_ = false;  // proven correct, so a modem self-reset may send it again
    if (this->pin_record_.sends != 0) {
      this->pin_record_.sends = 0;
      this->save_pin_record_();
    }
    return SimResult::SIM_RESULT_READY;
  }
  if (strstr(status, "PUK") != nullptr) {
    ESP_LOGE(TAG, "SIM is PUK locked; unlock it in a phone");
    return SimResult::SIM_RESULT_FATAL;
  }
  // Only the plain SIM PIN takes pin_code; SIM PIN2 and the PH-* locks need other credentials
  if (strncmp(status, "SIM PIN", 7) != 0 || status[7] == '2') {
    if (strstr(status, "PIN") != nullptr) {
      ESP_LOGE(TAG, "SIM lock %s needs a credential pin_code cannot provide", status);
      return SimResult::SIM_RESULT_FATAL;
    }
    ESP_LOGW(TAG, "SIM not ready: %s", status);  // e.g. NOT READY or NOT INSERTED
    this->set_warning_(WARN_SIM);
    return SimResult::SIM_RESULT_RETRY;
  }
  this->clear_warning_(WARN_SIM);  // a SIM that asks for its PIN is present and readable
  if (this->pin_code_ == nullptr) {
    ESP_LOGE(TAG, "SIM requires a PIN but pin_code is not set");
    return SimResult::SIM_RESULT_FATAL;
  }
  if (this->pin_record_.rejected) {
    ESP_LOGE(TAG, "The SIM rejected this pin_code before; not sending it again");
    return SimResult::SIM_RESULT_PIN_REJECTED;
  }
  if (this->pin_sent_) {
    // Some SIMs take seconds to unlock, so AT+CPIN? is asked again for a while
    if (millis() - this->pin_sent_at_ < PIN_UNLOCK_WAIT_MS) {
      return SimResult::SIM_RESULT_RETRY;
    }
    // The modem never said the PIN was wrong, so nothing is remembered; this attempt stops here rather than
    // sending the PIN blind a second time, and the reboot watchdog stays armed
    ESP_LOGE(TAG, "SIM still locked after sending the PIN; check the SIM");
    return SimResult::SIM_RESULT_PIN_UNCONFIRMED;
  }
  // Each send is counted in flash before the PIN goes out, so reboots cannot keep spending the SIM's attempts on a
  // PIN it never confirmed; READY clears the count. The count is handed to loop(), which writes it and reports back
  if (!this->pin_send_armed_) {
    if (this->pin_record_.sends >= PIN_MAX_UNCONFIRMED_SENDS) {
      ESP_LOGE(TAG, "The PIN went out %u times without the SIM confirming it; not sending it again",
               this->pin_record_.sends);
      return SimResult::SIM_RESULT_PIN_GIVEN_UP;
    }
    this->pin_record_.sends++;
    this->pin_send_armed_ = true;
    this->save_pin_record_();
    return SimResult::SIM_RESULT_RETRY;  // the save wakes the task
  }
  const uint8_t saved = this->pin_save_state_.load(std::memory_order_acquire);
  if (saved == 0) {
    return SimResult::SIM_RESULT_RETRY;  // still being written
  }
  this->pin_send_armed_ = false;
  if (saved != 1) {
    ESP_LOGE(TAG, "The PIN attempt could not be recorded; not sending it");
    return SimResult::SIM_RESULT_PIN_GIVEN_UP;
  }
  this->pin_sent_ = true;
  this->pin_sent_at_ = millis();
  ESP_LOGD(TAG, "Unlocking SIM with PIN...");
  char cmd[AT_COMMAND_SIZE];
  snprintf(cmd, sizeof(cmd), "AT+CPIN=\"%s\"", this->pin_code_);
  const command_result result = this->command_raw_(cmd, output, PIN_TIMEOUT_MS);
  if (result == command_result::OK) {
    return SimResult::SIM_RESULT_RETRY;  // AT+CPIN? confirms READY on the next pass
  }
  // "incorrect password" (+CME ERROR: 16, or the text in verbose mode) is a refused PIN; so is any ERROR when the
  // modem gives no codes, since a blind resend could PUK-lock the SIM. A PUK code or another error is not remembered
  if (result == command_result::FAIL &&
      (output.find("+CME ERROR: 16") != std::string::npos || output.find("incorrect password") != std::string::npos ||
       (!this->cme_numeric_ && output.find("+CME ERROR") == std::string::npos))) {
    ESP_LOGE(TAG, "The SIM rejected the PIN; check pin_code");
    this->pin_record_.rejected = true;
    this->save_pin_record_();
    return SimResult::SIM_RESULT_PIN_REJECTED;
  }
  if (output.find("+CME ERROR: 12") != std::string::npos) {
    ESP_LOGE(TAG, "SIM is PUK locked; unlock it in a phone");
    return SimResult::SIM_RESULT_FATAL;
  }
  // The raw reply starts with the echoed command, so only the error code is logged, never the PIN
  const size_t code = output.find("+CME ERROR");
  ESP_LOGW(TAG, "PIN not accepted yet (%s); AT+CPIN? decides",
           result == command_result::TIMEOUT ? LOG_STR_LITERAL("no answer")
           : code != std::string::npos       ? output.c_str() + code
                                             : LOG_STR_LITERAL("ERROR"));
  return SimResult::SIM_RESULT_RETRY;
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
