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
#include <cctype>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
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
// A modem in command mode answers the ATE0 sent before the mode detection within this time
static constexpr uint32_t ECHO_OFF_TIMEOUT_MS = 100;
// The +++ escape needs a second of silence before it; esp_modem waits for the answer after it
static constexpr uint32_t ESCAPE_GUARD_MS = 1100;
// A SIM7670 left in CMUX by an ESP reset ignores every frame until this many flag bytes overflow its receive buffer;
// it then reinitialises its AT channel within about 2 s
static constexpr size_t CMUX_FLUSH_BYTES = 4096;
static constexpr uint32_t CMUX_FLUSH_SETTLE_MS = 3000;
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
static constexpr uint8_t ATTACH_UNKNOWN_POLLS = 15;  // about a minute of polls
// AT+CGATT=1 once registered; SIMCom documents up to 75 s
static constexpr uint32_t ATTACH_TIMEOUT_MS = 75000;
// AT+CEREG? <stat> values
static constexpr int REGISTRATION_NOT_SEARCHING = 0;
static constexpr int REGISTRATION_HOME = 1;
static constexpr int REGISTRATION_SEARCHING = 2;
static constexpr int REGISTRATION_DENIED = 3;
static constexpr int REGISTRATION_ROAMING = 5;
static constexpr int REGISTRATION_UNKNOWN = -1;      // no readable answer
static constexpr int REGISTRATION_UNSUPPORTED = -2;  // neither AT+CEREG nor AT+CREG
// SIM status polls that fail before a warning, and between repeats of it
static constexpr uint8_t SIM_WARN_AFTER = 10;
static constexpr uint8_t SIM_WARN_EVERY = 60;
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
                     "PIN record not saved", "APN or radio setup failed", "No network attachment",
                     "Network registration denied", "PPP link failed", "Modem teardown failed");

// Indexed by the AT+CEREG? <stat> value
PROGMEM_STRING_TABLE(RegistrationStrings, "not searching", "registered", "searching", "denied", "unknown", "roaming",
                     "other");
static const LogString *registration_to_str(int registration) {
  if (registration == REGISTRATION_UNSUPPORTED) {
    return LOG_STR("not supported");
  }
  if (registration < 0) {
    return LOG_STR("not reported");
  }
  return RegistrationStrings::get_log_str(static_cast<uint8_t>(registration), RegistrationStrings::LAST_INDEX);
}

static bool is_registered(int registration) {
  return registration == REGISTRATION_HOME || registration == REGISTRATION_ROAMING;
}

// The <stat> after the first comma of the line starting with prefix (+CREG: and friends), or -1
static int registration_from_reply(const std::string &reply, const char *prefix) {
  // The query answer is "<n>,<stat>"; a one-field line is an unsolicited notification and is skipped
  for (size_t line = reply.find(prefix); line != std::string::npos; line = reply.find(prefix, line + 1)) {
    const char *start = reply.c_str() + line + strlen(prefix);
    char *end = nullptr;
    strtol(start, &end, 10);
    if (end == start || *end != ',') {
      continue;
    }
    start = end + 1;
    const auto stat = static_cast<int32_t>(strtol(start, &end, 10));
    if (end != start) {
      return stat;
    }
  }
  return -1;
}

// OK, or FAIL once an ERROR line has ended (the +CME ERROR reason may still be on its way), else TIMEOUT
// Only a complete final result line counts, so ERROR inside a payload line (+CEER: PROTOCOL_ERROR) does not. With
// skip_first, the first line is a cut-off fragment of a reply longer than the buffer.
static command_result reply_result(const char *data, size_t len, bool skip_first) {
  const char *end = data + len;
  const char *line = data;
  if (skip_first) {
    const auto *newline = static_cast<const char *>(memchr(data, '\n', len));
    line = newline != nullptr ? newline + 1 : end;
  }
  while (line < end) {
    const auto *newline = static_cast<const char *>(memchr(line, '\n', end - line));
    if (newline == nullptr) {
      break;  // the line has not ended yet
    }
    size_t size = newline - line;
    if (size != 0 && line[size - 1] == '\r') {
      size--;
    }
    if (size == 2 && memcmp(line, "OK", 2) == 0) {
      return command_result::OK;
    }
    if ((size == 5 && memcmp(line, "ERROR", 5) == 0) || (size >= 11 && memcmp(line, "+CME ERROR:", 11) == 0) ||
        (size >= 11 && memcmp(line, "+CMS ERROR:", 11) == 0)) {
      return command_result::FAIL;
    }
    line = newline + 1;
  }
  return command_result::TIMEOUT;
}

// Commands whose arguments are PINs or passwords, which must never reach the log
static constexpr const char *const SECRET_COMMANDS[] = {"+CPIN=", "+CPIN2=", "+CLCK=", "+CPWD="};
static constexpr size_t LOGGABLE_COMMAND_SIZE = 32;

// Offset just past the '=' of the first secret command in text, in any case, or 0 when there is none
static size_t find_secret_args(const char *text, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (text[i] != '+') {
      continue;
    }
    for (const char *secret : SECRET_COMMANDS) {
      const size_t size = strlen(secret);
      if (len - i >= size && strncasecmp(text + i, secret, size) == 0) {
        return i + size;
      }
    }
  }
  return 0;
}

// The command with the arguments of a secret command hidden
static const char *loggable_command(const char *cmd, char (&buf)[LOGGABLE_COMMAND_SIZE]) {
  const size_t args = find_secret_args(cmd, strlen(cmd));
  if (args == 0) {
    return cmd;
  }
  snprintf(buf, sizeof(buf), "%.*s<hidden>", static_cast<int>(args), cmd);
  return buf;
}

// Drops the arguments of every secret command a modem with echo on sent back in a flattened reply
static void hide_secret_args(std::string &reply) {
  size_t from = 0;
  while (const size_t args = find_secret_args(reply.data() + from, reply.size() - from)) {
    const size_t start = from + args;
    size_t end = start;
    bool quoted = false;
    while (end < reply.size() && (quoted || reply[end] != ' ')) {
      quoted ^= reply[end] == '"';
      end++;
    }
    reply.erase(start, end - start);
    from = start;
  }
}

// Puts a raw reply on one line for the log and drops the final OK
static void flatten_reply(std::string &reply) {
  std::replace_if(reply.begin(), reply.end(), [](char c) { return c == '\r' || c == '\n'; }, ' ');
  auto trim_end = [&reply]() {
    const size_t end = reply.find_last_not_of(' ');
    reply.resize(end == std::string::npos ? 0 : end + 1);
  };
  trim_end();
  const size_t len = reply.size();
  if (len >= 2 && reply.compare(len - 2, 2, "OK") == 0 && (len == 2 || reply[len - 3] == ' ')) {
    reply.resize(len - 2);
    trim_end();
  }
  reply.erase(0, reply.find_first_not_of(' '));
}

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
    this->publish_readings_(CSQ_UNKNOWN, CSQ_UNKNOWN, NETWORK_MODE_NO_SERVICE);
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
  this->reply_.reserve(REPLY_SIZE);
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
  if (!this->setup_check(this->uart_port_ != UART_NUM_MAX, LOG_STR("No free UART port for modem")))
    return;

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
  } else {
    this->answered_ = false;  // silent for minutes, so the wiring hint applies again
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
  // With echo on (SIM7670G after power-up), the modem sends esp_modem's PPP probe back and the detection takes it for
  // a PPP reply; in PPP or CMUX these bytes are dropped as a bad frame
  std::string reply;
  this->dce_->at("ATE0", reply, ECHO_OFF_TIMEOUT_MS);
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
        // A modem left in data mode before PPP started, or stuck in CMUX, answers none of the probes; both recoveries
        // go out at the rate the detection used
        if (this->current_baud_rate_ != detected) {
          const esp_err_t err = uart_set_baudrate(static_cast<uart_port_t>(this->uart_port_), detected);
          if (err == ESP_OK) {
            this->current_baud_rate_ = detected;
          } else {
            ESP_LOGW(TAG, "UART rejected baud rate %d (%s); data mode and CMUX recovery skipped", detected,
                     esp_err_to_name(err));
          }
        }
        // The escape before the flush is what a stuck SIM7670G responds to; the order is measured, not arbitrary
        if (this->current_baud_rate_ == detected && (this->escape_data_mode_() || this->flush_cmux_())) {
          exited = true;
          break;
        }
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
  // The modem answers AT, so it is in command mode. A failed data mode exit leaves esp_modem's UART side in data mode,
  // from where its later switch to CMUX fails at once, so its mode is set to match; this sends nothing to the modem.
  if (!this->dce_->set_mode(modem_mode::RESUME_COMMAND_MODE)) {
    ESP_LOGW(TAG, "esp_modem refused command mode; the switch to PPP may fail");
  }
  this->probe_backoff_.store(0, std::memory_order_relaxed);
  // esp_modem keeps the last line before OK as the reply, which can be the echoed command
  if (this->dce_->set_echo(false) != command_result::OK) {
    ESP_LOGW(TAG, "Failed to disable command echo");
  }
  // Verbose +CME ERROR text, so a failed command or a missing SIM says why; a modem without it keeps plain ERROR
  this->verbose_errors_ = this->dce_->config_mobile_termination_error(2) == command_result::OK;
  if (!this->verbose_errors_) {
    ESP_LOGW(TAG, "Modem refused verbose errors (AT+CMEE=2)");
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
  this->answered_ = true;
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
  this->sim_polls_ = 0;
  return ModemState::MODEM_STATE_CONNECTING;
}

ModemState ModemComponent::handle_state_connecting_() {
  if (this->connect_step_ == ConnectStep::CONNECT_STEP_START) {
    // ENABLING just synced; a command that timed out comes back here unsure of the modem
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
    this->idle_polls_ = 0;
    this->unknown_polls_ = 0;
  }

  int attached = 0;
  if (this->dce_->get_network_attachment_state(attached) != command_result::OK) {
    ESP_LOGW(TAG, "Failed to read the network attachment state");
    this->set_fault_(ModemFault::MODEM_FAULT_NOT_ATTACHED);
    this->request_resync_();
    return this->retry_(ModemState::MODEM_STATE_CONNECTING);
  }
  if (!attached) {
    const int registration = this->read_registration_();
    if (this->resync_) {
      ESP_LOGW(TAG, "No answer to a registration query");
      this->set_fault_(ModemFault::MODEM_FAULT_NOT_ATTACHED);
      return this->retry_(ModemState::MODEM_STATE_CONNECTING);
    }
    // A modem that has reported "not searching" for a few polls in a row (attachment switched off) gets the request
    // too. Right after CFUN=1 it briefly reports not searching, which is when the request would hold the AT channel;
    // any other or unreadable state starts the count again.
    this->idle_polls_ = registration == REGISTRATION_NOT_SEARCHING ? static_cast<uint8_t>(this->idle_polls_ + 1) : 0;
    const bool idle = this->idle_polls_ >= ATTACH_DIAG_EVERY;
    // A state that cannot be read or means nothing here holds the request back only for a while
    const bool unknown = !is_registered(registration) && registration != REGISTRATION_SEARCHING &&
                         registration != REGISTRATION_DENIED && registration != REGISTRATION_NOT_SEARCHING &&
                         registration != REGISTRATION_UNSUPPORTED;
    this->unknown_polls_ = unknown ? static_cast<uint8_t>(this->unknown_polls_ + 1) : 0;
    const bool unknown_too_long = this->unknown_polls_ >= ATTACH_UNKNOWN_POLLS;
    if (this->connect_step_ == ConnectStep::CONNECT_STEP_RADIO_ON &&
        (is_registered(registration) || registration == REGISTRATION_UNSUPPORTED || idle || unknown_too_long)) {
      if (unknown_too_long) {
        ESP_LOGI(TAG, "Registration state %d (%s) for %u polls; requesting the network attachment anyway", registration,
                 LOG_STR_ARG(registration_to_str(registration)), this->unknown_polls_);
        this->unknown_polls_ = 0;
      }
      // Before registration some modems hold the AT channel for minutes on this (SIM7080G: up to 141 s) and
      // drop every command meanwhile; LTE attaches with the registration anyway
      const command_result attach = this->at_reply_("AT+CGATT=1", ATTACH_TIMEOUT_MS);
      if (attach == command_result::TIMEOUT) {
        ESP_LOGW(TAG, "No answer to the network attachment request");
        this->request_resync_();
        return this->retry_(ModemState::MODEM_STATE_CONNECTING);
      }
      if (attach != command_result::OK) {
        ESP_LOGW(TAG, "Network attachment request refused: %s", this->reply_.c_str());
      }
      this->connect_step_ = ConnectStep::CONNECT_STEP_ATTACH_REQUESTED;
      return this->retry_(ModemState::MODEM_STATE_CONNECTING);
    }
    if (this->attach_polls_++ % ATTACH_DIAG_EVERY == 0) {
      int rssi = 99, ber = 99, mode = -1;  // -1 stays when the query fails
      this->dce_->get_signal_quality(rssi, ber);
      this->dce_->get_network_system_mode(mode);
      ESP_LOGW(TAG, "Modem not attached yet: registration %d (%s), network type %d, CSQ %d, BER %d", registration,
               LOG_STR_ARG(registration_to_str(registration)), mode, rssi, ber);
      if (unknown) {
        ESP_LOGW(TAG, "Network attachment request held back while the registration state is unknown (%u of %u polls)",
                 this->unknown_polls_, ATTACH_UNKNOWN_POLLS);
      }
      if (registration == REGISTRATION_DENIED) {
        // The modem's reason tells a rejected APN (ESM failure) from a SIM the network does not serve
        const command_result reason_read = this->at_reply_("AT+CEER");
        const char *prefix = reason_read == command_result::FAIL ? LOG_STR_LITERAL("AT+CEER failed: ") : "";
        const char *reason = reason_read == command_result::TIMEOUT ? LOG_STR_LITERAL("no answer to AT+CEER")
                             : this->reply_.empty()                 ? LOG_STR_LITERAL("no reason given")
                                                                    : this->reply_.c_str();
        ESP_LOGW(TAG,
                 "The network refused registration (%s%s); check the APN and that the SIM's plan covers this "
                 "modem's network types",
                 prefix, reason);
      }
      this->connect_step_ = ConnectStep::CONNECT_STEP_RADIO_ON;  // request the attachment again
    }
    this->set_fault_(registration == REGISTRATION_DENIED ? ModemFault::MODEM_FAULT_REGISTRATION_DENIED
                                                         : ModemFault::MODEM_FAULT_NOT_ATTACHED);
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

bool ModemComponent::escape_data_mode_() {
  delay(ESCAPE_GUARD_MS);  // NOLINT
  if (this->dce_->set_command_mode() != command_result::OK) {
    return false;
  }
  ESP_LOGW(TAG, "Modem was left in data mode; escaped to command mode");
  return true;
}

bool ModemComponent::flush_cmux_() {
  // With RTS/CTS a silent modem can hold CTS off; the flag bytes would then stay queued and block every later write
  if (this->rts_pin_ != nullptr) {
    ESP_LOGW(TAG, "CMUX recovery is not available with RTS/CTS flow control");
    return false;
  }
  static constexpr uint8_t CMUX_FLAG = 0xF9;
  uint8_t flags[64];
  memset(flags, CMUX_FLAG, sizeof(flags));
  const auto port = static_cast<uart_port_t>(this->uart_port_);
  for (size_t sent = 0; sent < CMUX_FLUSH_BYTES; sent += sizeof(flags)) {
    if (this->dte_->write(flags, sizeof(flags)) != static_cast<int>(sizeof(flags))) {
      ESP_LOGW(TAG, "CMUX flush: short write after %u bytes", static_cast<unsigned>(sent));
      return false;
    }
  }
  // 10 bits per byte, plus a margin
  const uint32_t send_ms = CMUX_FLUSH_BYTES * 10 * 1000 / this->current_baud_rate_ + 100;
  if (uart_wait_tx_done(port, pdMS_TO_TICKS(send_ms)) != ESP_OK) {
    ESP_LOGW(TAG, "CMUX flush: transmit not finished after %u ms", static_cast<unsigned>(send_ms));
    return false;
  }
  delay(CMUX_FLUSH_SETTLE_MS);  // NOLINT
  if (this->dce_->sync() != command_result::OK) {
    return false;
  }
  ESP_LOGW(TAG, "Modem was stuck in CMUX; reset it to command mode");
  return true;
}

// A command timed out: the next pass checks with a sync that the modem still answers
void ModemComponent::request_resync_() {
  this->connect_step_ = ConnectStep::CONNECT_STEP_START;
  this->resync_ = true;
}

// AT+CEREG covers LTE; on 2G and 3G the network registration is in AT+CREG, and AT+CGATT=1 is answered quickly once
// it is set. Returns the <stat> that tells most, -1 when neither query was answered, or REGISTRATION_UNSUPPORTED.
int ModemComponent::read_registration_() {
  // A bare ERROR means the command is not supported; a +CME ERROR is a temporary failure
  const command_result lte_read = this->at_reply_("AT+CEREG?");
  const int lte =
      lte_read == command_result::OK ? registration_from_reply(this->reply_, "+CEREG:") : REGISTRATION_UNKNOWN;
  if (lte == REGISTRATION_UNKNOWN && lte_read != command_result::TIMEOUT) {
    ESP_LOGD(TAG, "AT+CEREG? gave no state: %s", this->reply_.c_str());
  }
  if (is_registered(lte)) {
    return lte;
  }
  const bool lte_unsupported = this->unsupported_reply_(lte_read);
  const command_result legacy_read = this->at_reply_("AT+CREG?");
  const int legacy =
      legacy_read == command_result::OK ? registration_from_reply(this->reply_, "+CREG:") : REGISTRATION_UNKNOWN;
  if (legacy == REGISTRATION_UNKNOWN && legacy_read != command_result::TIMEOUT) {
    ESP_LOGD(TAG, "AT+CREG? gave no state: %s", this->reply_.c_str());
  }
  const bool legacy_unsupported = this->unsupported_reply_(legacy_read);
  // A multimode modem can be registered or still searching on one network type while the other refuses it
  if (is_registered(legacy)) {
    return legacy;
  }
  // An unanswered query leaves the state open, so the modem is checked with a sync instead
  if (lte_read == command_result::TIMEOUT || legacy_read == command_result::TIMEOUT) {
    this->request_resync_();
    return REGISTRATION_UNKNOWN;
  }
  if (lte == REGISTRATION_SEARCHING || legacy == REGISTRATION_SEARCHING) {
    return REGISTRATION_SEARCHING;
  }
  if (lte == REGISTRATION_DENIED || legacy == REGISTRATION_DENIED) {
    return REGISTRATION_DENIED;
  }
  if (lte_unsupported && legacy_unsupported) {
    return REGISTRATION_UNSUPPORTED;
  }
  return lte != REGISTRATION_UNKNOWN ? lte : legacy;
}

// Whether a failed query says the command is not supported. Without verbose errors a temporary failure is a bare
// ERROR too, so only an error that names it (+CME ERROR: 4) counts then.
bool ModemComponent::unsupported_reply_(command_result result) const {
  if (result != command_result::FAIL) {
    return false;
  }
  const size_t error = this->reply_.find("+CME ERROR:");
  if (error == std::string::npos) {
    return this->verbose_errors_;
  }
  const char *code = this->reply_.c_str() + error + 11;
  while (*code == ' ') {
    code++;
  }
  return strncasecmp(code, "operation not supported", 23) == 0 || (code[0] == '4' && !isdigit(code[1]));
}

void ModemComponent::not_responding_(int baud) {
  if (this->answered_) {
    // It answered before, so the wiring is fine; a long command or a modem restart keeps it silent for minutes
    ESP_LOGW(TAG, "Modem not responding at %d baud; it may be busy or restarting", baud);
  } else {
    ESP_LOGW(TAG, "Modem not responding at %d baud. Check power, PWRKEY and TX/RX wiring.", baud);
  }
  this->set_fault_(ModemFault::MODEM_FAULT_NO_RESPONSE);
}

void ModemComponent::sim_not_ready_(const char *reason) {
  // A missing or broken SIM never gets ready; the status is polled every second, so only some polls warn
  if (++this->sim_polls_ == SIM_WARN_AFTER + SIM_WARN_EVERY) {
    this->sim_polls_ = SIM_WARN_AFTER;
  }
  if (this->sim_polls_ == SIM_WARN_AFTER) {
    ESP_LOGW(TAG, "SIM not ready: %s. Check that a working SIM is inserted",
             reason[0] != '\0' ? reason : LOG_STR_LITERAL("no reply"));
  }
  this->set_fault_(ModemFault::MODEM_FAULT_SIM_NOT_READY);
}

#ifdef USE_MODEM_SEND_AT
bool ModemComponent::send_at(const std::string &cmd, uint32_t timeout) { return this->send_at(cmd.c_str(), timeout); }

bool ModemComponent::send_at(const char *cmd, uint32_t timeout) {
  char buf[LOGGABLE_COMMAND_SIZE];
  if (!this->task_.is_created()) {
    ESP_LOGW(TAG, "Modem not ready, cannot send AT command: %s", loggable_command(cmd, buf));
    return false;
  }
  if (this->at_pending_.load(std::memory_order_acquire)) {
    ESP_LOGW(TAG, "AT command '%s' dropped; the previous one is not sent yet", loggable_command(cmd, buf));
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

// at() keeps no text from a failed command, so the raw reply is collected into reply_ to show the +CME ERROR.
// reply_ is reserved once and never grows, so the regular polls do not allocate for the reply.
command_result ModemComponent::at_reply_(const char *cmd, uint32_t timeout) {
  std::string line = cmd;
  line += '\r';
  std::string &reply = this->reply_;
  reply.clear();
  const command_result result = this->dce_->command(
      line,
      [&reply, truncated = false](const uint8_t *data, size_t len) mutable {
        // A UART reply grows in place and comes back whole each time; CMUX hands over each frame on its own
        const char *text = reinterpret_cast<const char *>(data);
        if (len >= reply.size() && reply.compare(0, reply.size(), text, reply.size()) == 0) {
          reply.clear();
        }
        // A reply longer than the buffer keeps its newest bytes, where the final OK or ERROR line is
        const size_t capacity = reply.capacity();
        if (len >= capacity) {
          reply.assign(text + len - capacity, capacity);
          truncated = true;
        } else {
          if (reply.size() + len > capacity) {
            reply.erase(0, reply.size() + len - capacity);
            truncated = true;
          }
          reply.append(text, len);
        }
        return reply_result(reply.data(), reply.size(), truncated);
      },
      timeout);
  flatten_reply(reply);
  return result;
}

command_result ModemComponent::at_(const char *cmd, uint32_t timeout) {
  const command_result result = this->at_reply_(cmd, timeout);
  hide_secret_args(this->reply_);
  char buf[LOGGABLE_COMMAND_SIZE];
  if (result == command_result::OK) {
    ESP_LOGI(TAG, "AT '%s' -> OK: %s", loggable_command(cmd, buf), this->reply_.c_str());
  } else {
    ESP_LOGW(TAG, "AT '%s' -> %s: %s", loggable_command(cmd, buf), LOG_STR_ARG(result_to_str(result)),
             this->reply_.c_str());
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
  // The raw reply keeps the +CME ERROR text a missing SIM gives; status below points into it
  const command_result result = this->at_reply_("AT+CPIN?");
  std::string &output = this->reply_;
  if (result == command_result::TIMEOUT) {
    // The next pass checks with a sync whether the modem or only the SIM interface is silent
    ESP_LOGD(TAG, "No answer to the SIM status request");
    this->sim_not_ready_(LOG_STR_LITERAL("no answer to AT+CPIN?"));
    this->request_resync_();
    return SimResult::SIM_RESULT_RETRY;
  }
  if (result != command_result::OK) {
    // A SIM that is still starting answers with an error for a second or so
    ESP_LOGD(TAG, "SIM status not readable yet: %s", output.c_str());
    this->sim_not_ready_(output.c_str());
    return SimResult::SIM_RESULT_RETRY;
  }
  // Anything else, such as an echoed command, is not a status and must not reach the lock checks below
  size_t start = output.find("+CPIN:");
  if (start != std::string::npos) {
    start = output.find_first_not_of(' ', start + sizeof("+CPIN:") - 1);
    // A URC in the same reply follows on its own line, which flatten_reply() turned into two spaces
    const size_t end = output.find("  ", start);
    if (end != std::string::npos) {
      output.resize(end);
    }
  }
  const StringRef status = start != std::string::npos ? StringRef(output.c_str() + start) : StringRef();
  if (status.empty()) {
    ESP_LOGD(TAG, "SIM status not reported yet: %s", output.c_str());
    this->sim_not_ready_(output.c_str());
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
    ESP_LOGD(TAG, "SIM not ready: %s", status.c_str());  // e.g. NOT READY or NOT INSERTED
    this->sim_not_ready_(status.c_str());
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
  int rssi = CSQ_UNKNOWN, ber = CSQ_UNKNOWN;
  const bool signal_ok = this->dce_->get_signal_quality(rssi, ber) == command_result::OK;
  this->poll_rssi_ = static_cast<uint8_t>(signal_ok && rssi >= 0 && rssi < CSQ_UNKNOWN ? rssi : CSQ_UNKNOWN);
  this->poll_ber_ = static_cast<uint8_t>(signal_ok && ber >= 0 && ber < CSQ_UNKNOWN ? ber : CSQ_UNKNOWN);
#endif
#ifdef USE_MODEM_NETWORK_TYPE
  int mode = -1;
  const bool mode_ok = this->dce_->get_network_system_mode(mode) == command_result::OK;
  this->poll_mode_ = static_cast<uint8_t>(mode_ok && mode >= 0 && mode < NETWORK_MODE_NONE ? mode : NETWORK_MODE_NONE);
#endif
}

#ifdef USE_MODEM_NETWORK_TYPE
// AT+CNSMOD values 0-8 on the SIM7600 family; anything else shows as Unknown
PROGMEM_STRING_TABLE(NetworkModeStrings, "No service", "GSM", "GPRS", "EDGE", "WCDMA", "HSDPA", "HSUPA", "HSPA", "LTE",
                     "Unknown");
#endif

// Raw AT answers: CSQ rssi and ber, and the CNSMOD mode (past the table shows as Unknown)
void ModemComponent::publish_readings_([[maybe_unused]] uint8_t rssi, [[maybe_unused]] uint8_t ber,
                                       [[maybe_unused]] uint8_t mode) {
#ifdef USE_MODEM_SIGNAL
  PUBLISH_STATE(this->rssi_sensor_, rssi <= CSQ_RSSI_MAX ? -113.0f + 2.0f * rssi : NAN);
  PUBLISH_STATE(this->ber_sensor_, ber < std::size(BER_PERCENT) ? BER_PERCENT[ber] : NAN);
#endif
#ifdef USE_MODEM_NETWORK_TYPE
  PUBLISH_STATE(this->network_type_text_sensor_,
                NetworkModeStrings::get_progmem_str(mode, NetworkModeStrings::LAST_INDEX));
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
