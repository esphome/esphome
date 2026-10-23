#ifdef USE_ESP32
#include "modem_component.h"
#include "soc/soc_caps.h"
#include <driver/uart.h>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/network/util.h"

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_netif_ppp.h>

#include <algorithm>
#include <cinttypes>
#include <cmath>
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
// A silent modem is not going to answer the next second, so its probes are spaced out, doubling up to a minute
static constexpr uint32_t PROBE_RETRY_DELAY_MS = 5000;
static constexpr uint32_t PROBE_RETRY_MAX_MS = 60000;
// How long the task waits for the main loop to run on_sync before attaching
static constexpr uint32_t SYNC_ACK_TIMEOUT_MS = 5000;
// esp_modem's command strings and the log lines are built on this stack
static constexpr uint32_t TASK_STACK_SIZE = 5120;
#ifdef USE_MODEM_PIN_CODE
// AT+CPIN= can take a few seconds; the SIM may need longer still before AT+CPIN? says READY
static constexpr uint32_t PIN_TIMEOUT_MS = 5000;
static constexpr uint32_t PIN_UNLOCK_WAIT_MS = 10000;
// Sends of one pin_code the SIM has not confirmed with READY, across reboots, before it is not sent again
static constexpr uint8_t PIN_MAX_UNCONFIRMED_SENDS = 2;
#endif
// The attachment is polled every 4 s; the signal and operator queries only every 4th poll
static constexpr uint32_t ATTACH_POLL_MS = 4000;
static constexpr uint8_t ATTACH_DIAG_EVERY = 4;
static constexpr int RADIO_STATE_RF_OFF = 4;  // CFUN=4: transmit and receive off, SIM still powered
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

// Indexed by ModemFault
PROGMEM_STRING_TABLE(ModemFaultStrings, "", "Modem not responding", "Configured baud rate failed",
                     "init_at command failed", "SIM not ready", "SIM locked", "SIM PIN not sent; check pin_code",
                     "PIN record not saved", "APN or radio setup failed", "No network attachment", "PPP link failed",
                     "Modem teardown failed");

// Indexed by esp_modem::command_result
PROGMEM_STRING_TABLE(CommandResultStrings, "OK", "FAIL", "TIMEOUT");
static const LogString *result_to_str(command_result result) {
  return CommandResultStrings::get_log_str(static_cast<uint8_t>(result), CommandResultStrings::LAST_INDEX);
}

ModemComponent::ModemComponent(InternalGPIOPin *tx_pin, InternalGPIOPin *rx_pin, const char *model, DceFactory factory,
                               const char *apn)
    : tx_pin_(tx_pin), rx_pin_(rx_pin), model_(model), dce_factory_(factory), apn_(apn) {
  global_modem_component = this;
}

void ModemComponent::enable() {
  ESP_LOGI(TAG, "Setting modem target state to CONNECTED");
  this->target_state_.store(ModemState::MODEM_STATE_CONNECTED);
  this->probe_backoff_.store(0, std::memory_order_relaxed);
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

// The task is the only writer, so a load and a store do; a byte-sized read-modify-write is a library call on Xtensa
void ModemComponent::set_fault_(ModemFault fault) {
  // A wrong baud_rate or init_at is kept apart, so it stays visible once the modem connects
  if (fault == ModemFault::MODEM_FAULT_BAUD || fault == ModemFault::MODEM_FAULT_INIT_AT) {
    this->config_fault_.store(fault, std::memory_order_relaxed);
  }
  if (this->fault_.load(std::memory_order_relaxed) != fault) {
    this->fault_.store(fault, std::memory_order_relaxed);
    this->hand_over_(PENDING_STATUS);
  }
}

// The fault that ended the session says more than a failed cleanup after it
void ModemComponent::set_teardown_fault_() {
  if (this->fault_.load(std::memory_order_relaxed) == ModemFault::MODEM_FAULT_NONE) {
    this->set_fault_(ModemFault::MODEM_FAULT_TEARDOWN);
  }
}

void ModemComponent::clear_config_fault_(ModemFault fault) {
  if (this->config_fault_.load(std::memory_order_relaxed) == fault) {
    this->config_fault_.store(ModemFault::MODEM_FAULT_NONE, std::memory_order_relaxed);
    this->hand_over_(PENDING_STATUS);
  }
}

void ModemComponent::publish_status_() {
  ModemFault fault = this->fault_.load(std::memory_order_relaxed);
#ifdef USE_MODEM_PIN_CODE
  if (this->pin_record_unsaved_) {
    fault = ModemFault::MODEM_FAULT_PIN_RECORD;
  }
#endif
  if (fault == ModemFault::MODEM_FAULT_NONE) {
    fault = this->config_fault_.load(std::memory_order_relaxed);
  }
  if (fault == ModemFault::MODEM_FAULT_NONE) {
    this->status_clear_warning();
    this->status_clear_error();
    return;
  }
  const LogString *message = ModemFaultStrings::get_log_str(static_cast<uint8_t>(fault), ModemFaultStrings::LAST_INDEX);
  // These stop the modem until the SIM or pin_code is fixed
  if (fault == ModemFault::MODEM_FAULT_SIM_LOCKED || fault == ModemFault::MODEM_FAULT_PIN_NOT_SENT ||
      fault == ModemFault::MODEM_FAULT_PIN_RECORD) {
    this->status_clear_warning();
    this->status_set_error(message);
  } else {
    this->status_clear_error();
    this->status_set_warning(message);
  }
}

void ModemComponent::loop() {
  const uint32_t pending = this->pending_.exchange(0, std::memory_order_acquire);
  if (pending & (PENDING_ON_SYNC | PENDING_ON_CONNECT | PENDING_ON_DISCONNECT)) {
    // The task waits for the sync hand-over, so a pending disconnect came before a pending sync; connected now
    // means a pending disconnect also came before the pending connect
    const bool disconnect_first =
        (pending & PENDING_ON_DISCONNECT) && (this->is_connected() || (pending & PENDING_ON_SYNC));
    if (disconnect_first) {
      this->on_state_callback_.call(ModemState::MODEM_STATE_DISCONNECTING);
    }
    if (pending & PENDING_ON_SYNC) {
      this->on_state_callback_.call(ModemState::MODEM_STATE_SYNCED);
      this->sync_acked_.store(1, std::memory_order_release);  // on_sync finished, so the task may attach
      this->wake_task_();
    }
    if (pending & PENDING_ON_CONNECT) {
      this->dump_connect_params_();
      this->on_state_callback_.call(ModemState::MODEM_STATE_CONNECTED);
    }
    if ((pending & PENDING_ON_DISCONNECT) && !disconnect_first) {
      this->on_state_callback_.call(ModemState::MODEM_STATE_DISCONNECTING);
    }
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
#ifdef USE_MODEM_PIN_CODE
  // Written through at once: a power cycle before the periodic flush would send the PIN again
  if (pending & PENDING_SAVE_PIN) {
    // The task leaves pin_record_ alone until pin_save_state_ reports the outcome
    const bool saved = this->pin_pref_.save(&this->pin_record_) && global_preferences->sync();
    if (!saved) {
      ESP_LOGE(TAG, "Failed to save the PIN record");
    }
    // A power cycle could send a refused PIN again, so a failed save shows until a later one succeeds
    if (this->pin_record_unsaved_ != !saved) {
      this->pin_record_unsaved_ = !saved;
      this->publish_status_();
    }
    this->pin_save_state_.store(saved ? PinSave::PIN_SAVE_DONE : PinSave::PIN_SAVE_FAILED, std::memory_order_release);
    this->wake_task_();
  }
#endif
#ifdef USE_MODEM_POLL
  if (pending & PENDING_ON_DISCONNECT) {
    // A lost link must not keep showing the last healthy reading, nor one taken before it dropped
    this->poll_unpublished_.store(0, std::memory_order_release);
    this->publish_readings_(99, 99, 0);
  } else if (this->poll_unpublished_.load(std::memory_order_acquire)) {
    this->publish_readings_(this->poll_rssi_, this->poll_ber_, this->poll_mode_);
    this->poll_unpublished_.store(0, std::memory_order_release);
  }
#endif
#ifdef USE_MODEM_URC
  if (this->urc_pending_.load(std::memory_order_acquire)) {
    this->urc_text_sensor_->publish_state(this->urc_line_);
    this->urc_pending_.store(0, std::memory_order_release);
  }
#endif
  // A hand-over after these checks re-enables the loop through enable_loop_soon_any_context()
  if (this->pending_.load(std::memory_order_acquire) == 0) {
    this->disable_loop();
  }
}

void ModemComponent::setup() {
  this->pref_ = global_preferences->make_preference<int>(BAUD_PREF_HASH);
  this->pref_.load(&this->restored_baud_);
#ifdef USE_MODEM_PIN_CODE
  this->pin_pref_ = global_preferences->make_preference<PinRecord>(PIN_PREF_HASH);
  if (!this->pin_pref_.load(&this->pin_record_)) {
    // Also the first boot with pin_code; after a flash fault the send count starts over
    ESP_LOGD(TAG, "No PIN record stored");
  }
  // A changed pin_code starts with a clean record and may be sent again
  const uint32_t pin_hash = fnv1_hash(this->pin_code_);
  if (this->pin_record_.hash != pin_hash) {
    this->pin_record_ = PinRecord{pin_hash, 0};
    if (!this->pin_pref_.save(&this->pin_record_)) {
      ESP_LOGW(TAG, "Failed to clear the PIN record");
    }
  }
#else
  // A build without pin_code forgets the count, so flashing once without it clears the record
  {
    auto pin_pref = global_preferences->make_preference<PinRecord>(PIN_PREF_HASH);
    PinRecord record{};
    if (pin_pref.load(&record) && record.hash != 0) {
      record = PinRecord{};
      if (!pin_pref.save(&record)) {
        ESP_LOGW(TAG, "Failed to clear the PIN record");
      }
    }
  }
#endif

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
  // chunk is consumed and a complete line is handed to loop() in urc_line_.
  this->dce_->set_enhanced_urc([this](const DTE::UrcBufferInfo &buffer_info) {
    // The response parser owns the buffer while a command runs; URCs in that window are best effort,
    // and a line started before it would otherwise be glued to the next one
    if (buffer_info.is_command_active) {
      this->urc_reader_.reset();
      return DTE::UrcConsumeInfo{DTE::UrcConsumeResult::CONSUME_NONE, 0};
    }
    bool dropped = false;
    for (size_t i = 0; i < buffer_info.new_data_size; i++) {
      const uint8_t c = buffer_info.new_data_start[i];
      if (this->urc_pending_.load(std::memory_order_acquire)) {
        // The line loop() has not published yet lives in the reader's buffer, which a dropped line leaves alone
        dropped |= c != '\r' && c != '\n';
        this->urc_reader_.discard_line();
      }
      const char *line = this->urc_reader_.feed(c);
      this->urc_reader_.log_if_overflowed(TAG);
      if (line != nullptr && line[0] != '\0') {
        this->urc_line_ = line;
        this->urc_pending_.store(1, std::memory_order_release);
        this->enable_loop_soon_any_context();
      }
    }
    if (dropped) {
      ESP_LOGW(TAG, "URC dropped; the previous one is not published yet");
    }
    return DTE::UrcConsumeInfo{DTE::UrcConsumeResult::CONSUME_ALL, 0};
  });
#endif
  if (!this->task_.create(ModemComponent::task_entry, "modem", TASK_STACK_SIZE, this, 1, false)) {
    this->fail_setup_(LOG_STR("Task creation failed"));
  }
}

void ModemComponent::dump_config() {
#ifdef USE_MODEM_PIN_CODE
  const char *pin_code = LOG_STR_LITERAL("Yes (not shown)");
#else
  const char *pin_code = LOG_STR_LITERAL("No");
#endif
  ESP_LOGCONFIG(TAG,
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
                this->model_, this->apn_, pin_code, this->tx_pin_->get_pin(), this->rx_pin_->get_pin(),
                static_cast<int>(this->uart_port_), this->current_baud_rate_, this->tx_buffer_size_,
                this->rx_buffer_size_, this->dte_buffer_size_, this->timeout_, YESNO(this->is_enabled()));
#ifdef USE_MODEM_POLL
  ESP_LOGCONFIG(TAG, "  Update interval: %" PRIu32 " ms", this->update_interval_);
#endif
#ifdef USE_MODEM_SIGNAL
  LOG_SENSOR("  ", "RSSI", this->rssi_sensor_);
  LOG_SENSOR("  ", "BER", this->ber_sensor_);
#endif
#ifdef USE_MODEM_NETWORK_TYPE
  LOG_TEXT_SENSOR("  ", "Network type", this->network_type_text_sensor_);
#endif
  this->dump_connect_params_();
}

void ModemComponent::task_entry(void *arg) { static_cast<ModemComponent *>(arg)->run_task_(); }

void ModemComponent::run_task_() {
  while (true) {
#ifdef USE_MODEM_SEND_AT
    // A command from send_at() goes out between state steps, never in the middle of a sequence
    if (this->at_pending_.load(std::memory_order_acquire)) {
      this->at_(this->at_command_, this->at_timeout_);
      this->at_pending_.store(0, std::memory_order_release);
    }
#endif
    this->step_delay_ms_ = 0;
    this->transition_to_(this->compute_next_state_());
    // Nothing to do while disabled or connected: enable(), disable() and the PPP events wake the task
    const bool parked = this->component_state_.load() == this->target_state_.load();
    TickType_t wait = parked ? portMAX_DELAY : pdMS_TO_TICKS(this->step_delay_ms_);
#ifdef USE_MODEM_POLL
    // Like send_at(), the poll goes out between state steps; while connected the next poll ends the wait
    if (parked && this->is_connected()) {
      int32_t remaining = static_cast<int32_t>(this->next_poll_ms_ - millis());
      if (remaining <= 0) {
        // A result loop() has not published yet is not overwritten; that poll is skipped
        if (!this->poll_unpublished_.load(std::memory_order_acquire)) {
          this->poll_();
          this->poll_unpublished_.store(1, std::memory_order_release);
          this->enable_loop_soon_any_context();
        }
        this->next_poll_ms_ = millis() + this->update_interval_;
        remaining = static_cast<int32_t>(this->update_interval_);
      }
      wait = pdMS_TO_TICKS(remaining);
    }
#endif
    ulTaskNotifyTake(pdTRUE, wait);
  }
}

uint32_t ModemComponent::next_probe_delay_() {
  const uint8_t backoff = this->probe_backoff_.load(std::memory_order_relaxed);
  const uint32_t delay_ms = std::min(PROBE_RETRY_DELAY_MS << backoff, PROBE_RETRY_MAX_MS);
  if (delay_ms < PROBE_RETRY_MAX_MS) {
    this->probe_backoff_.store(backoff + 1, std::memory_order_relaxed);
  }
  return delay_ms;
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
#ifdef USE_MODEM_PIN_CODE
      // A new attempt may send the PIN again; a counted send that has not gone out yet still does
      if (this->pin_step_ == PinStep::PIN_STEP_SENT) {
        this->pin_step_ = PinStep::PIN_STEP_IDLE;
      }
#endif
      return ModemState::MODEM_STATE_ENABLING;
    case ModemState::MODEM_STATE_ENABLING:
      return this->handle_state_enabling_();
    case ModemState::MODEM_STATE_SYNCED:
      return this->handle_state_synced_();
    case ModemState::MODEM_STATE_CONNECTING:
      return this->handle_state_connecting_();
    case ModemState::MODEM_STATE_WAIT_IP:
      return this->handle_state_wait_ip_();
    case ModemState::MODEM_STATE_CONNECTED:
      if (this->got_ip_.load(std::memory_order_acquire)) {
        return ModemState::MODEM_STATE_CONNECTED;
      }
      ESP_LOGW(TAG, "Lost IP; PPP error %u", this->ppp_error_.load(std::memory_order_acquire));
      this->ppp_error_.store(0, std::memory_order_release);
      this->set_fault_(ModemFault::MODEM_FAULT_PPP);
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

  // A detection proves the modem answers; leaving CMUX or data mode has to be checked with a sync
  bool exited = true;
  switch (mode) {
    case modem_mode::CMUX_MANUAL_MODE:
      // A data session survived the ESP reboot; the radio off step below ends it
      ESP_LOGD(TAG, "Modem warm reboot. Exiting CMUX");
      if (!this->dce_->set_mode(modem_mode::CMUX_MANUAL_EXIT)) {
        ESP_LOGW(TAG, "Failed to exit CMUX");
      }
      break;

    case modem_mode::UNDEF: {
      // The detection above probed the current rate; the other known rates are tried in this pass
      const int detected = this->current_baud_rate_;
      exited = false;
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
          this->set_fault_(ModemFault::MODEM_FAULT_BAUD);
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
        // The detection at the current rate already failed; a UART that refused every other rate set BAUD
        if (!probed && !rejected) {
          this->not_responding_(detected);
        }
        return this->retry_(ModemState::MODEM_STATE_ENABLING, this->next_probe_delay_());
      }
      ESP_LOGD(TAG, "Modem responded at baud %d", this->current_baud_rate_);
      break;
    }

    case modem_mode::COMMAND_MODE:
      exited = false;
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

  if (exited && this->dce_->sync() != command_result::OK) {
    // The detection proved the modem answers, so the CMUX or data mode exit is what failed; the exit can block
    // for 20 s, so it backs off like the probe
    ESP_LOGW(TAG, "Modem did not return to command mode");
    this->set_fault_(ModemFault::MODEM_FAULT_TEARDOWN);
    return this->retry_(ModemState::MODEM_STATE_ENABLING, this->next_probe_delay_());
  }
  this->probe_backoff_.store(0, std::memory_order_relaxed);
  // esp_modem keeps the last line before OK as the reply, which can be the echoed command
  if (this->dce_->set_echo(false) != command_result::OK) {
    ESP_LOGW(TAG, "Failed to disable command echo");
  }

  int radio_state = 0;
  if (this->dce_->get_radio_state(radio_state) != command_result::OK) {
    ESP_LOGW(TAG, "Failed to read the radio state; skipping the radio off step");
  } else if (radio_state != RADIO_STATE_RF_OFF) {
    // Drops a session left by a warm reboot; unlike CFUN=0, which powers the SIM down on the SIM7600, CFUN=4
    // keeps the SIM readable for the PIN and the APN. A modem that refuses is used as it is.
    if (this->dce_->set_radio_state(RADIO_STATE_RF_OFF) != command_result::OK) {
      ESP_LOGW(TAG, "Failed to turn the radio off");
    }
  }

  if (this->baud_rate_ != 0 && this->baud_rate_ != this->current_baud_rate_) {
    ESP_LOGD(TAG, "Setting baud rate: %d -> %d", this->current_baud_rate_, this->baud_rate_);
    if (this->dce_->set_baud(this->baud_rate_) == command_result::OK) {
      delay(200);  // NOLINT
      if (uart_set_baudrate(static_cast<uart_port_t>(this->uart_port_), this->baud_rate_) != ESP_OK) {
        ESP_LOGE(TAG, "UART rejected baud rate %d", this->baud_rate_);
        this->set_fault_(ModemFault::MODEM_FAULT_BAUD);
        return ModemState::MODEM_STATE_DISCONNECTING;
      }
      this->current_baud_rate_ = this->baud_rate_;
      if (this->dce_->sync() != command_result::OK) {
        ESP_LOGE(TAG, "Failed to sync modem at baud rate %d.", this->baud_rate_);
        this->set_fault_(ModemFault::MODEM_FAULT_BAUD);
        return ModemState::MODEM_STATE_DISCONNECTING;
      }
      ESP_LOGI(TAG, "Modem synced at baud rate %d.", this->current_baud_rate_);
    } else {
      // The modem still answers at the probed rate, so a rejected rate is not worth a retry loop
      ESP_LOGW(TAG, "Modem rejected baud rate %d. Staying at %d.", this->baud_rate_, this->current_baud_rate_);
      this->set_fault_(ModemFault::MODEM_FAULT_BAUD);
    }
  }

  // The rate in use is the wanted one, so a baud fault from this or an earlier attempt no longer applies
  if (this->baud_rate_ == 0 || this->baud_rate_ == this->current_baud_rate_) {
    this->clear_config_fault_(ModemFault::MODEM_FAULT_BAUD);
  }
  if (this->restored_baud_ != this->current_baud_rate_) {
    this->restored_baud_ = this->current_baud_rate_;
    this->hand_over_(PENDING_SAVE_BAUD);
  }

  // The DTE side was set up with the UART; the modem side has to be asked for RTS/CTS as well
  if (this->rts_pin_ != nullptr && this->dce_->set_flow_control(2, 2) != command_result::OK) {
    ESP_LOGW(TAG, "Modem refused hardware flow control; UART overruns are possible");
  }
  this->modem_synced_ = true;
  return ModemState::MODEM_STATE_SYNCED;
}

ModemState ModemComponent::handle_state_synced_() {
  // transition_to_() gave on_sync its head start; a command it queued went out at the top of this pass
  bool failed = false;
  for (uint8_t i = 0; i != this->init_at_count_; i++) {
    failed |= this->at_(this->init_at_commands_[i]) != command_result::OK;  // at_() logs the failure
  }
  if (failed) {
    this->set_fault_(ModemFault::MODEM_FAULT_INIT_AT);
  } else {
    this->clear_config_fault_(ModemFault::MODEM_FAULT_INIT_AT);
  }
  this->connect_step_ = ConnectStep::CONNECT_STEP_START;
  return ModemState::MODEM_STATE_CONNECTING;
}

ModemState ModemComponent::handle_state_connecting_() {
  if (this->connect_step_ == ConnectStep::CONNECT_STEP_START) {
    // ENABLING just synced; only a failed attachment read comes back here unsure of the modem
    if (this->resync_) {
      this->resync_ = false;
      if (this->dce_->sync() != command_result::OK) {
        ESP_LOGW(TAG, "Modem not synced during network init");
        return ModemState::MODEM_STATE_ENABLING;
      }
    }
    // ENABLING left the radio at CFUN=4, where the SIM answers but the modem cannot attach yet
    switch (this->prepare_sim_()) {
      case SimResult::SIM_RESULT_READY:
        break;
      case SimResult::SIM_RESULT_RETRY:
        return this->retry_(ModemState::MODEM_STATE_CONNECTING);
      case SimResult::SIM_RESULT_LOCKED:
        // A reboot would send nothing more, so the reboot timer is cancelled
        this->target_state_.store(ModemState::MODEM_STATE_DISABLED);
        this->hand_over_(PENDING_CONNECT_TIMEOUT);
        return ModemState::MODEM_STATE_DISCONNECTING;
      case SimResult::SIM_RESULT_HALT:
        // Retrying now could burn the SIM's PIN attempts; the reboot timer stays armed, since a reboot may
        // still open the SIM
        this->target_state_.store(ModemState::MODEM_STATE_DISABLED);
        return ModemState::MODEM_STATE_DISCONNECTING;
    }
    // The APN has to be in the PDP context before the radio comes on and the modem attaches; esp_modem only
    // sends it when PPP starts
    esp_modem::PdpContext pdp(this->apn_);
    if (this->dce_->set_pdp_context(pdp) != command_result::OK) {
      ESP_LOGW(TAG, "Failed to set the APN");
      this->set_fault_(ModemFault::MODEM_FAULT_SETUP);
      return this->retry_(ModemState::MODEM_STATE_CONNECTING);
    }
    if (this->dce_->set_radio_state(1) != command_result::OK) {
      ESP_LOGW(TAG, "Failed to turn the radio on");
      this->set_fault_(ModemFault::MODEM_FAULT_SETUP);
      return this->retry_(ModemState::MODEM_STATE_CONNECTING);
    }
    this->connect_step_ = ConnectStep::CONNECT_STEP_RADIO_ON;
    this->attach_polls_ = 0;
  }

  if (this->connect_step_ == ConnectStep::CONNECT_STEP_RADIO_ON) {
    // A modem still searching for a network refuses this; the attachment poll asks again
    if (this->dce_->set_network_attachment_state(1) != command_result::OK) {
      ESP_LOGD(TAG, "Network attachment request refused");
    }
    this->connect_step_ = ConnectStep::CONNECT_STEP_ATTACH_REQUESTED;
  }

  int attached = 0;
  if (this->dce_->get_network_attachment_state(attached) != command_result::OK) {
    ESP_LOGW(TAG, "Failed to read the network attachment state");
    this->set_fault_(ModemFault::MODEM_FAULT_NOT_ATTACHED);
    this->connect_step_ = ConnectStep::CONNECT_STEP_START;  // start over with a sync on the next pass
    this->resync_ = true;
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
      this->connect_step_ = ConnectStep::CONNECT_STEP_RADIO_ON;  // request the attachment again
    }
    this->set_fault_(ModemFault::MODEM_FAULT_NOT_ATTACHED);
    return this->retry_(ModemState::MODEM_STATE_CONNECTING, ATTACH_POLL_MS);
  }

  ESP_LOGI(TAG, "Modem initialized and ready, starting PPP");
  // A late event from the last session must not count
  this->got_ip_.store(0, std::memory_order_release);
  this->ppp_error_.store(0, std::memory_order_release);
  if (!this->dce_->set_mode(modem_mode::CMUX_MODE)) {
    ESP_LOGE(TAG, "Failed to enter PPP. Resetting modem.");
    this->set_fault_(ModemFault::MODEM_FAULT_PPP);
    return ModemState::MODEM_STATE_DISCONNECTING;
  }
  this->state_started_ms_ = millis();
  return ModemState::MODEM_STATE_WAIT_IP;
}

ModemState ModemComponent::handle_state_wait_ip_() {
  if (this->got_ip_.load(std::memory_order_acquire)) {
    this->set_fault_(ModemFault::MODEM_FAULT_NONE);
    return ModemState::MODEM_STATE_CONNECTED;
  }
  // A PPP failure (wrong APN, authentication, dead peer) ends the wait at once; the event logged the code
  if (this->ppp_error_.load(std::memory_order_acquire) != 0) {
    this->ppp_error_.store(0, std::memory_order_release);
    this->set_fault_(ModemFault::MODEM_FAULT_PPP);
    return ModemState::MODEM_STATE_DISCONNECTING;
  }
  const uint32_t elapsed = millis() - this->state_started_ms_;
  if (elapsed < WAIT_IP_TIMEOUT_MS) {
    // The IP event wakes the task sooner
    return this->retry_(ModemState::MODEM_STATE_WAIT_IP, WAIT_IP_TIMEOUT_MS - elapsed);
  }
  ESP_LOGE(TAG, "Unable to get IP address");
  this->set_fault_(ModemFault::MODEM_FAULT_PPP);
  return ModemState::MODEM_STATE_DISCONNECTING;
}

ModemState ModemComponent::handle_state_disconnecting_() {
  // Each teardown command holds the task for up to 20 s, so a modem that never answered is skipped
  if (!this->modem_synced_) {
    return ModemState::MODEM_STATE_DISABLED;
  }
  this->modem_synced_ = false;
  // COMMAND_MODE stops PPP and leaves CMUX; the manual exit only applies to a warm reboot
  if (this->dce_->get_mode() != modem_mode::COMMAND_MODE && !this->dce_->set_mode(modem_mode::COMMAND_MODE)) {
    // The next enable() probes the mode again and retries the exit; the fault keeps this visible
    ESP_LOGW(TAG, "Failed to leave data mode");
    this->set_teardown_fault_();
  }
  // Turn the radio off; waiting for the lost IP event could take up to 2 min. A reconnect keeps the SIM powered
  // so it is not unlocked again; only disable() powers it down.
  const bool reconnect = this->target_state_.load() == ModemState::MODEM_STATE_CONNECTED;
  if (this->dce_->set_radio_state(reconnect ? RADIO_STATE_RF_OFF : 0) == command_result::OK) {
    ESP_LOGI(TAG, "Radio off");
  } else {
    ESP_LOGW(TAG, "Radio off failed");
    this->set_teardown_fault_();
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
  // The automations run on the main loop
  if (next_state == ModemState::MODEM_STATE_SYNCED) {
    // on_sync may queue an AT command or disable the modem; it gets a bounded head start, and loop() wakes the
    // task once it ran. An earlier enable() or a send_at() inside on_sync also notifies, so the flag decides.
    this->sync_acked_.store(0, std::memory_order_release);
    this->hand_over_(PENDING_ON_SYNC);
    const uint32_t started = millis();
    while (this->sync_acked_.load(std::memory_order_acquire) == 0) {
      const uint32_t elapsed = millis() - started;
      if (elapsed >= SYNC_ACK_TIMEOUT_MS) {
        ESP_LOGW(TAG, "on_sync did not run within %u ms; continuing", static_cast<unsigned>(SYNC_ACK_TIMEOUT_MS));
        break;
      }
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(SYNC_ACK_TIMEOUT_MS - elapsed));
    }
  } else if (next_state == ModemState::MODEM_STATE_CONNECTED) {
    this->hand_over_(PENDING_ON_CONNECT | PENDING_CONNECT_TIMEOUT);
#ifdef USE_MODEM_POLL
    this->next_poll_ms_ = millis();  // the first poll runs right after connecting
#endif
  } else if (next_state == ModemState::MODEM_STATE_DISCONNECTING) {
    // The lost IP event can lag by minutes; a quick re-enable must wait for a fresh address
    this->got_ip_.store(0, std::memory_order_release);
    this->step_delay_ms_ = COMMAND_TIMEOUT_MS;  // let the link settle before the first pass
    // on_disconnect pairs with on_connect, so a failed attempt that never connected does not fire it; a lost
    // connection starts the reconnect watchdog again
    if (lost) {
      this->hand_over_(PENDING_ON_DISCONNECT | PENDING_CONNECT_TIMEOUT);
    }
  }
}

void ModemComponent::not_responding_(int baud) {
  ESP_LOGW(TAG, "Modem not responding at %d baud. Check power, PWRKEY and TX/RX wiring.", baud);
  this->set_fault_(ModemFault::MODEM_FAULT_NO_RESPONSE);
}

#ifdef USE_MODEM_SEND_AT
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

#endif

command_result ModemComponent::at_(const char *cmd, uint32_t timeout) {
  std::string output;
  const command_result result = this->dce_->at(cmd, output, timeout);
  if (result == command_result::OK) {
    ESP_LOGI(TAG, "AT '%s' -> OK: %s", cmd, output.c_str());
  } else {
    ESP_LOGW(TAG, "AT '%s' -> %s: %s", cmd, LOG_STR_ARG(result_to_str(result)), output.c_str());
  }
  return result;
}

// esp_modem's read_pin() takes NOT READY for READY and SIM PIN2 for the SIM PIN, so the status is read here
SimResult ModemComponent::prepare_sim_() {
#ifdef USE_MODEM_PIN_CODE
  // Every write to pin_record_ happens below, so waiting here keeps the task off it while loop() saves it
  if (this->pin_save_state_.load(std::memory_order_acquire) == PinSave::PIN_SAVE_PENDING) {
    return SimResult::SIM_RESULT_RETRY;  // the save wakes the task
  }
#endif
  std::string output;
  if (this->dce_->at("AT+CPIN?", output, COMMAND_TIMEOUT_MS) != command_result::OK) {
    // A SIM that is still starting answers with an error for a second or so
    ESP_LOGD(TAG, "SIM status not readable yet: %s", output.c_str());
    this->set_fault_(ModemFault::MODEM_FAULT_SIM_NOT_READY);
    return SimResult::SIM_RESULT_RETRY;
  }
  // Anything else, such as an echoed command, is not a status and must not reach the lock checks below
  const char *found = strstr(output.c_str(), "+CPIN:");
  if (found != nullptr) {
    found += sizeof("+CPIN:") - 1;
    while (*found == ' ') {
      found++;
    }
  }
  const StringRef status = found != nullptr ? StringRef(found) : StringRef();
  if (status.empty()) {
    ESP_LOGD(TAG, "SIM status not reported yet: %s", output.c_str());
    this->set_fault_(ModemFault::MODEM_FAULT_SIM_NOT_READY);
    return SimResult::SIM_RESULT_RETRY;
  }
  ESP_LOGD(TAG, "SIM: %s", status.c_str());

  // A prefix match: "NOT READY" contains READY but does not start with it
  if (status.starts_with("READY")) {
#ifdef USE_MODEM_PIN_CODE
    this->pin_step_ = PinStep::PIN_STEP_IDLE;  // proven correct, so a modem self-reset may send it again
    if (this->pin_record_.sends != 0) {
      this->pin_record_.sends = 0;
      this->save_pin_record_();
    }
#endif
    return SimResult::SIM_RESULT_READY;
  }
  if (strstr(status.c_str(), "PUK") != nullptr) {
    ESP_LOGE(TAG, "SIM is PUK locked; unlock it in a phone");
    this->set_fault_(ModemFault::MODEM_FAULT_SIM_LOCKED);
    return SimResult::SIM_RESULT_LOCKED;
  }
  // Only the plain SIM PIN takes pin_code; SIM PIN2 and the PH-* locks need other credentials
  if (!status.starts_with("SIM PIN") || status.starts_with("SIM PIN2")) {
    if (strstr(status.c_str(), "PIN") != nullptr) {
      ESP_LOGE(TAG, "SIM lock %s needs a credential pin_code cannot provide", status.c_str());
      this->set_fault_(ModemFault::MODEM_FAULT_SIM_LOCKED);
      return SimResult::SIM_RESULT_LOCKED;
    }
    ESP_LOGW(TAG, "SIM not ready: %s", status.c_str());  // e.g. NOT READY or NOT INSERTED
    this->set_fault_(ModemFault::MODEM_FAULT_SIM_NOT_READY);
    return SimResult::SIM_RESULT_RETRY;
  }
#ifdef USE_MODEM_PIN_CODE
  return this->send_pin_();
#else
  ESP_LOGE(TAG, "SIM requires a PIN but pin_code is not set");
  this->set_fault_(ModemFault::MODEM_FAULT_SIM_LOCKED);
  return SimResult::SIM_RESULT_LOCKED;
#endif
}

#ifdef USE_MODEM_PIN_CODE
void ModemComponent::save_pin_record_() {
  this->pin_save_state_.store(PinSave::PIN_SAVE_PENDING, std::memory_order_release);
  this->hand_over_(PENDING_SAVE_PIN);
}

// Each send is counted in flash before the PIN goes out, so reboots cannot keep spending the SIM's attempts on a
// PIN it never confirmed; READY clears the count and a refused send uses it up
SimResult ModemComponent::send_pin_() {
  switch (this->pin_step_) {
    case PinStep::PIN_STEP_IDLE:
      if (this->pin_record_.sends >= PIN_MAX_UNCONFIRMED_SENDS) {
        ESP_LOGE(TAG, "The SIM never confirmed this pin_code; not sending it again");
        this->set_fault_(ModemFault::MODEM_FAULT_PIN_NOT_SENT);
        return SimResult::SIM_RESULT_LOCKED;
      }
      this->pin_record_.sends++;
      this->pin_step_ = PinStep::PIN_STEP_SAVING;
      this->save_pin_record_();
      return SimResult::SIM_RESULT_RETRY;

    case PinStep::PIN_STEP_SAVING: {
      // prepare_sim_() waited for the outcome
      if (this->pin_save_state_.load(std::memory_order_acquire) != PinSave::PIN_SAVE_DONE) {
        ESP_LOGE(TAG, "The PIN attempt could not be recorded; not sending it");
        this->pin_record_.sends--;
        this->pin_step_ = PinStep::PIN_STEP_IDLE;
        this->set_fault_(ModemFault::MODEM_FAULT_PIN_RECORD);
        return SimResult::SIM_RESULT_HALT;
      }
      this->pin_step_ = PinStep::PIN_STEP_SENT;
      this->state_started_ms_ = millis();
      std::string cmd = "AT+CPIN=\"";
      cmd += this->pin_code_;
      cmd += '"';
      if (this->at_(cmd.c_str(), PIN_TIMEOUT_MS) == command_result::FAIL) {
        // A blind resend could PUK-lock the SIM, so a refusal uses up the count until pin_code changes
        ESP_LOGE(TAG, "The SIM refused the PIN; check pin_code");
        this->pin_record_.sends = PIN_MAX_UNCONFIRMED_SENDS;
        this->save_pin_record_();
        this->set_fault_(ModemFault::MODEM_FAULT_PIN_NOT_SENT);
        return SimResult::SIM_RESULT_LOCKED;
      }
      return SimResult::SIM_RESULT_RETRY;  // AT+CPIN? confirms READY on the next pass
    }

    case PinStep::PIN_STEP_SENT:
      // Some SIMs take seconds to unlock, so AT+CPIN? is asked again for a while
      if (millis() - this->state_started_ms_ < PIN_UNLOCK_WAIT_MS) {
        return SimResult::SIM_RESULT_RETRY;
      }
      ESP_LOGE(TAG, "SIM still locked after sending the PIN; check the SIM");
      if (this->pin_record_.sends >= PIN_MAX_UNCONFIRMED_SENDS) {
        this->set_fault_(ModemFault::MODEM_FAULT_PIN_NOT_SENT);
        return SimResult::SIM_RESULT_LOCKED;
      }
      // This attempt stops rather than sending the PIN blind again; a reboot sends it once more
      this->set_fault_(ModemFault::MODEM_FAULT_SIM_LOCKED);
      return SimResult::SIM_RESULT_HALT;
  }
  return SimResult::SIM_RESULT_RETRY;
}
#endif

#ifdef USE_MODEM_POLL

#ifdef USE_MODEM_SIGNAL
// 3GPP TS 45.008 representative bit error rates for the RXQUAL classes AT+CSQ reports
static constexpr float BER_PERCENT[] = {0.14f, 0.28f, 0.57f, 1.13f, 2.26f, 4.53f, 9.05f, 18.10f};
#endif

void ModemComponent::poll_() {
  // The task owns the poll_ answers while poll_unpublished_ is clear
#ifdef USE_MODEM_SIGNAL
  int rssi = 99, ber = 99;
  const bool signal_ok = this->dce_->get_signal_quality(rssi, ber) == command_result::OK;
  this->poll_rssi_ = static_cast<uint8_t>(signal_ok && rssi >= 0 && rssi < 99 ? rssi : 99);
  this->poll_ber_ = static_cast<uint8_t>(signal_ok && ber >= 0 && ber < 99 ? ber : 99);
#endif
#ifdef USE_MODEM_NETWORK_TYPE
  int mode = -1;
  const bool mode_ok = this->dce_->get_network_system_mode(mode) == command_result::OK;
  this->poll_mode_ = static_cast<uint8_t>(mode_ok && mode >= 0 && mode < 255 ? mode : 255);
#endif
}

#ifdef USE_MODEM_NETWORK_TYPE
// AT+CNSMOD values 0-8 on the SIM7600 family; anything else shows as Unknown
PROGMEM_STRING_TABLE(NetworkModeStrings, "No service", "GSM", "GPRS", "EDGE", "WCDMA", "HSDPA", "HSUPA", "HSPA", "LTE",
                     "Unknown");
#endif

// Raw AT answers: CSQ rssi and ber (99 = unknown) and the CNSMOD mode (past the table = Unknown)
void ModemComponent::publish_readings_([[maybe_unused]] uint8_t rssi, [[maybe_unused]] uint8_t ber,
                                       [[maybe_unused]] uint8_t mode) {
#ifdef USE_MODEM_SIGNAL
  if (this->rssi_sensor_ != nullptr) {
    this->rssi_sensor_->publish_state(rssi <= 31 ? -113.0f + 2.0f * rssi : NAN);
  }
  if (this->ber_sensor_ != nullptr) {
    this->ber_sensor_->publish_state(ber < std::size(BER_PERCENT) ? BER_PERCENT[ber] : NAN);
  }
#endif
#ifdef USE_MODEM_NETWORK_TYPE
  if (this->network_type_text_sensor_ != nullptr) {
    char name[NetworkModeStrings::MAX_LENGTH + 1];
    ESPHOME_strncpy_P(
        name,
        reinterpret_cast<ESPHOME_PGM_P>(NetworkModeStrings::get_progmem_str(mode, NetworkModeStrings::LAST_INDEX)),
        sizeof(name));
    name[sizeof(name) - 1] = '\0';
    this->network_type_text_sensor_->publish_state(name);
  }
#endif
}
#endif

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
