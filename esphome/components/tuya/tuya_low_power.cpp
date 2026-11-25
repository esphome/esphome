#include "esphome/core/defines.h"

#ifdef USE_TUYA_LOW_POWER

#include "tuya_uart.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#ifdef USE_NETWORK
#include "esphome/components/network/util.h"
#endif

namespace esphome::tuya {

ESPHOME_LOG_TAG(LP_TAG, "tuya.low_power");
// A version-3 MCU (firmware 1.0.9) reports changes made at the device only when asked.
static constexpr uint32_t LP_V3_STATUS_POLL_MS = 60000;
// Until its first report, the status query of a version-3 MCU is repeated when the answer was lost.
static constexpr uint32_t LP_V3_STATUS_RETRY_MS = 3000;
static constexpr uint32_t LP_COMMAND_DELAY_MS = 10;
// The MCU sleeps within a second of its last byte; after this long without one it is woken before a command.
static constexpr uint32_t LP_MCU_SILENT_MS = 200;
// After a malformed product info or a lost answer: 5 retries 500 ms apart, then one every 30 s.
static constexpr uint32_t LP_PRODUCT_RETRY_MS = 500;
static constexpr uint8_t LP_PRODUCT_RETRIES = 5;
static constexpr uint32_t LP_PRODUCT_SLOW_RETRY_MS = 30000;
// Pending writes; a newer write to the same datapoint replaces the queued one.
static constexpr size_t LP_MAX_QUEUED_COMMANDS = 8;
// Sleep requests of the MCU. The module does not sleep, so both are always answered with "busy".
static constexpr uint8_t LP_V0_SLEEP_CMD = 0x38;
static constexpr uint8_t LP_V3_SLEEP_CMD = 0x91;
static constexpr uint8_t LP_V3_SLEEP_REQUEST = 0x02;
static constexpr uint8_t LP_V3_BUSY_NETWORK = 0x02;

// The network status reuses the Wi-Fi status scheduler id of tuya_uart.cpp: a hub runs one protocol only.
static constexpr uint32_t LP_NETWORK_STATUS_INTERVAL_ID = 1;

// A queued write: CMD09, sent as CMD06 to a version-3 MCU (the version is known only from the product info).
static constexpr auto LP_WRITE_CMD = static_cast<TuyaCommandType>(0x09);
static bool is_low_power_write(const TuyaCommand &command) { return command.cmd == LP_WRITE_CMD; }
// The network state (CMD02), queued so it does not cross a write
static constexpr auto LP_NETWORK_CMD = static_cast<TuyaCommandType>(0x02);

// A datapoint list the decoder takes whole: types and sizes as handle_datapoints_() accepts them.
static bool valid_datapoints(const uint8_t *data, size_t len) {
  if (len < 4)
    return false;
  for (size_t offset = 0; offset < len;) {
    if (len - offset < 4)
      return false;
    const size_t size = encode_uint16(data[offset + 2], data[offset + 3]);
    const uint8_t type = data[offset + 1];
    if (size > len - offset - 4 || type > 5 || ((type == 1 || type == 4) && size != 1) || (type == 2 && size != 4) ||
        (type == 5 && size != 1 && size != 2 && size != 4) || (type == 1 && data[offset + 4] > 1))
      return false;
    offset += 4 + size;
  }
  return true;
}

void TuyaUART::setup_low_power_() {
  this->init_state_ = TuyaInitState::INIT_PRODUCT;
  if (this->mcu_wake_pin_ != nullptr) {
    this->mcu_wake_pin_->setup();
    this->mcu_wake_pin_->digital_write(false);  // Inactive logical level.
  }
  this->send_low_power_product_query_(false);
  this->set_interval(LP_NETWORK_STATUS_INTERVAL_ID, 1000, [this] { this->send_low_power_network_(); });
}

void TuyaUART::loop_low_power_() {
  if (!this->parent_->is_connected()) {
    this->mcu_wake_pulse_running_();  // A running wake pulse still ends on time
    this->expected_response_.reset();
    if (!this->lp_session_.faulted()) {
      this->lp_session_.command_timeout();
      this->lp_wire_.transport_fault();
      this->lp_network_status_ = 0xFF;
      ESP_LOGE(LP_TAG, "MCU UART unhealthy: MCU transactions cancelled until recovery");
    }
    // Once the UART is back, the product query restores the MCU link.
    this->begin_low_power_product_retry_(App.get_loop_component_start_time());
    return;
  }
  this->poll_low_power_product_probe_();
  this->poll_low_power_product_recovery_();
  switch (this->lp_session_.update(App.get_loop_component_start_time())) {
    case TuyaLowPowerSession::Event::EVENT_SYNCHRONIZED:
      this->finish_low_power_sync_();
      break;
    case TuyaLowPowerSession::Event::EVENT_TIMED_OUT:
      ESP_LOGW(LP_TAG, "Status or write report timeout; requesting the MCU state again");
      this->lp_network_status_ = 0xFF;
      break;
    default:
      break;
  }
  this->process_low_power_queue_();
  this->poll_low_power_v3_status_();
}

void TuyaUART::poll_low_power_v3_status_() {
  const bool synced = this->lp_session_.synchronized();
  // Queued writes wait for the status this asks for, so only other queued commands hold it back
  if (!this->lp_v3_ || !this->lp_v3_mode_acked_ || (synced && this->lp_v3_pushes_) || this->lp_session_.busy() ||
      !this->lp_wire_.idle() || this->reader_.receiving() ||
      !std::all_of(this->command_queue_.begin(), this->command_queue_.end(), is_low_power_write))
    return;
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->lp_v3_last_poll_ms_ < (synced ? LP_V3_STATUS_POLL_MS : LP_V3_STATUS_RETRY_MS))
    return;
  this->lp_v3_last_poll_ms_ = now;
  ESP_LOGV(LP_TAG, "Polling MCU status with CMD08");
  // Queued, so a sleeping MCU is woken first
  this->send_empty_command_(static_cast<TuyaCommandType>(0x08));
}

void TuyaUART::handle_low_power_frame_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len) {
  // Only a valid frame shows that the MCU is awake; noise on the line of a sleeping MCU does not
  this->lp_mcu_heard_ms_ = millis();
  // A late answer to an expired command must not complete a newer one.
  if (this->lp_wire_.overdue(App.get_loop_component_start_time()) && this->lp_wire_.accepts(command, len)) {
    ESP_LOGW(LP_TAG, "Discarding overdue MCU ACK CMD 0x%02X; recovery required", command);
    return;
  }
  // The answer may be another command than the request: CMD07 confirms a version-3 write (CMD06).
  if (this->lp_wire_.complete(command, len))
    this->expected_response_.reset();
  this->handle_low_power_command_(command, version, buffer, len);
  // During a recovery a frame shows that the MCU is awake: ask for the product info now, not at the next retry.
  const uint32_t now = App.get_loop_component_start_time();
  if (command != 0x01 && this->lp_product_retry_active_ && !this->lp_wire_.active() && !this->lp_session_.busy() &&
      this->lp_probe_pending_ == ProductProbe::PRODUCT_PROBE_NONE &&
      static_cast<int32_t>(now - this->lp_product_probe_ms_) >= static_cast<int32_t>(LP_PRODUCT_RETRY_MS)) {
    this->lp_product_probe_ms_ = now;
    this->lp_product_retry_at_ = now + LP_PRODUCT_RETRY_MS;
    this->send_product_query_now_(true);
  }
}

void TuyaUART::send_low_power_raw_command_(const TuyaCommand &command) {
  this->last_command_timestamp_ = millis();
  const auto cmd = static_cast<uint8_t>(command.cmd);
  const uint32_t now = App.get_loop_component_start_time();
  if (cmd == 0x01 || cmd == 0x02 || cmd == 0x06 || cmd == 0x08 || cmd == 0x09) {
    if (!this->lp_wire_.begin(cmd, now)) {
      ESP_LOGE(LP_TAG, "Refusing an overlapping MCU transaction");
      this->lp_session_.command_timeout();
      this->lp_wire_.transport_fault();
      this->begin_low_power_product_retry_(now);
      return;
    }
    this->expected_response_ = command.cmd;
  }
  if (cmd == 0x02 && command.payload.size() == 1 && command.payload[0] == 0x04 && !this->lp_v3_)
    this->lp_session_.begin_full_status(now);
  if (cmd == 0x06 && this->lp_v3_)
    this->lp_v3_last_poll_ms_ = now;  // Its CMD07 echo is a reply, not a report of its own.
  if (cmd == 0x09)
    this->lp_session_.begin_write(now);
  ESP_LOGV(LP_TAG, "Sending MCU CMD=0x%02X len=%u", cmd, static_cast<unsigned>(command.payload.size()));
  this->send_low_power_direct_(cmd, command.payload.data(), command.payload.size());
}

void TuyaUART::send_low_power_datapoint_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const uint8_t *data,
                                         size_t len) {
  std::vector<uint8_t> buffer{datapoint_id, static_cast<uint8_t>(datapoint_type), static_cast<uint8_t>(len >> 8),
                              static_cast<uint8_t>(len)};
  buffer.insert(buffer.end(), data, data + len);
  // Writes wait in the queue until the MCU's state is known; a newer value replaces a queued one.
  for (auto &queued : this->command_queue_) {
    if (is_low_power_write(queued) && queued.payload[0] == datapoint_id) {
      queued.payload = std::move(buffer);
      return;
    }
  }
  if (this->command_queue_.size() >= LP_MAX_QUEUED_COMMANDS) {
    ESP_LOGW(LP_TAG, "Too many pending MCU commands; dropping the DP%u write", static_cast<unsigned>(datapoint_id));
    return;
  }
  this->command_queue_.push_back(TuyaCommand{.cmd = LP_WRITE_CMD, .payload = std::move(buffer)});
}

void TuyaUART::begin_mcu_wake_pulse_() {
  if (this->mcu_wake_pin_ == nullptr || this->lp_mcu_wake_pulse_active_)
    return;
  this->mcu_wake_pin_->digital_write(true);
  // millis(), not the loop time: the pulse may start late in a loop pass and must last its full length.
  this->lp_mcu_wake_pulse_until_ = millis() + this->lp_product_.mcu_wake_pulse_ms;
  this->lp_mcu_wake_pulse_active_ = true;
}

// Ends a finished wake pulse. True while the pulse still runs: nothing may go to the MCU before it has woken.
bool TuyaUART::mcu_wake_pulse_running_() {
  if (!this->lp_mcu_wake_pulse_active_)
    return false;
  if (static_cast<int32_t>(millis() - this->lp_mcu_wake_pulse_until_) < 0)
    return true;
  this->mcu_wake_pin_->digital_write(false);
  this->lp_mcu_wake_pulse_active_ = false;
  // Counted from the end of the pulse, so the command goes out before the MCU falls asleep again.
  this->lp_mcu_woken_ms_ = millis();
  return false;
}

// direct_probe sends CMD01 at once, outside the quarantined queue, for a retry.
void TuyaUART::send_low_power_product_query_(bool direct_probe) {
  if (this->mcu_wake_pin_ == nullptr) {
    this->send_product_query_now_(direct_probe);
    return;
  }
  // Like the original module firmware, wake the MCU before CMD01; it goes out when the pulse is over.
  this->lp_probe_pending_ = direct_probe ? ProductProbe::PRODUCT_PROBE_DIRECT : ProductProbe::PRODUCT_PROBE_QUEUED;
  this->begin_mcu_wake_pulse_();
}

void TuyaUART::poll_low_power_product_probe_() {
  if (this->lp_probe_pending_ == ProductProbe::PRODUCT_PROBE_NONE || this->mcu_wake_pulse_running_())
    return;
  const bool direct_probe = this->lp_probe_pending_ == ProductProbe::PRODUCT_PROBE_DIRECT;
  this->lp_probe_pending_ = ProductProbe::PRODUCT_PROBE_NONE;
  this->send_product_query_now_(direct_probe);
}

void TuyaUART::send_product_query_now_(bool direct_probe) {
  if (direct_probe) {
    this->send_low_power_direct_(0x01, {});
  } else {
    this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
  }
}

void TuyaUART::begin_low_power_product_retry_(uint32_t now) {
  if (this->lp_product_retry_active_)
    return;
  this->lp_product_retry_active_ = true;
  this->lp_product_retries_ = LP_PRODUCT_RETRIES;
  this->lp_product_retry_at_ = now + LP_PRODUCT_RETRY_MS;
}

void TuyaUART::poll_low_power_product_recovery_() {
  if (!this->lp_product_retry_active_)
    return;
  const uint32_t now = App.get_loop_component_start_time();
  // Not during a report series or a write; the quarantined queue does not hold the retry back.
  const bool idle = !this->lp_session_.busy() && !this->lp_wire_.active() && !this->expected_response_.has_value() &&
                    !this->reader_.receiving();
  if (static_cast<int32_t>(now - this->lp_product_retry_at_) < 0 || !idle)
    return;
  if (this->lp_product_retries_ > 0) {
    --this->lp_product_retries_;
    this->lp_product_retry_at_ = now + LP_PRODUCT_RETRY_MS;
  } else {
    // The MCU may sleep without a wake line, or be gone: keep asking, more slowly, and keep listening.
    if (!this->status_has_warning()) {
      ESP_LOGW(LP_TAG, "MCU does not answer the product query; asking every %u s", LP_PRODUCT_SLOW_RETRY_MS / 1000);
    }
    this->status_set_warning(LOG_STR("MCU does not answer"));
    this->lp_product_retry_at_ = now + LP_PRODUCT_SLOW_RETRY_MS;
  }
  this->lp_product_probe_ms_ = now;
  this->send_low_power_product_query_(true);
}

void TuyaUART::process_low_power_queue_() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->mcu_wake_pulse_running_())
    return;
  // After the pulse check, which may stamp its end; the command timestamp of tuya_uart.cpp comes from millis() too.
  const uint32_t wire_now = millis();

  // A version-0 MCU reports some time after its ACK, so ACK and report have separate timeouts.
  const bool product_query = this->lp_wire_.active() && this->lp_wire_.command() == 0x01;
  if (this->lp_wire_.expire(now)) {
    this->expected_response_.reset();
    this->lp_session_.command_timeout();
    this->lp_network_status_ = 0xFF;
    // Recover with the direct CMD01 probe; a queued one would wait behind the quarantined queue.
    this->begin_low_power_product_retry_(now);
    ESP_LOGW(LP_TAG, "MCU command ACK timeout (product=%s); writes wait for a full resync", YESNO(product_query));
    // Unsent writes wait for the full resync, which no late ACK can reach; queued handshake commands are stale.
    auto &queue = this->command_queue_;
    queue.erase(std::remove_if(queue.begin(), queue.end(), [](const TuyaCommand &c) { return !is_low_power_write(c); }),
                queue.end());
  }

  if (this->lp_session_.busy() || this->lp_wire_.active() || this->lp_wire_.faulted() ||
      this->expected_response_.has_value() || this->reader_.receiving() || this->handling_frame_ ||
      static_cast<uint32_t>(wire_now - this->last_command_timestamp_) <= LP_COMMAND_DELAY_MS)
    return;
  // Before the product info only CMD01 may go out; writes wait until the MCU's state is known.
  auto next = std::find_if(this->command_queue_.begin(), this->command_queue_.end(), [this, now](const TuyaCommand &c) {
    if (!this->lp_session_.has_product())
      return c.cmd == TuyaCommandType::PRODUCT_QUERY;
    return !is_low_power_write(c) || (this->lp_session_.ready() && this->lp_session_.quiet(now));
  });
  if (next == this->command_queue_.end())
    return;
  // A silent MCU may sleep and listens only after a wake pulse; the command goes out once the pulse is over.
  if (this->mcu_wake_pin_ != nullptr && wire_now - this->lp_mcu_heard_ms_ > LP_MCU_SILENT_MS &&
      wire_now - this->lp_mcu_woken_ms_ > LP_MCU_SILENT_MS) {
    this->begin_mcu_wake_pulse_();
    return;
  }

  TuyaCommand command = std::move(*next);
  this->command_queue_.erase(next);
  if (this->lp_v3_ && is_low_power_write(command))
    command.cmd = static_cast<TuyaCommandType>(0x06);
  // A partial frame kept by an idle flush must not merge into the reply
  this->reader_.drop_kept();
  this->send_low_power_raw_command_(command);
}

// Sends one frame right away, outside the queue: ACKs and time replies must not wait behind a write.
void TuyaUART::send_low_power_direct_(uint8_t command, const uint8_t *data, size_t len) {
  if (len > 65535)
    return;
  // A partial frame kept by an idle flush must not merge into the reply
  this->reader_.drop_kept();
  const uint8_t header[] = {
      0x55, 0xAA, 0x00, command, static_cast<uint8_t>(len >> 8), static_cast<uint8_t>(len),
  };
  const uint8_t checksum = sum_bytes(header, sizeof(header)) + sum_bytes(data, len);
  this->write_array(header, sizeof(header));
  if (len != 0)
    this->write_array(data, len);
  this->write_byte(checksum);
}

void TuyaUART::send_low_power_network_() {
  if (!this->lp_session_.has_product())
    return;
  // The MCU reports only to a module "connected to the cloud" (0x04): sent whenever the network is up.
  uint8_t status = 0x02;
#ifdef USE_NETWORK
  if (network::is_connected())
    status = 0x04;
#endif
  if (status == this->lp_network_status_)
    return;
  this->lp_network_status_ = status;
  if (this->lp_v3_ && this->lp_v3_mode_acked_) {
    // Version 3 after the working-mode query (CMD02): the state is CMD03, queued so a sleeping MCU is woken.
    this->send_command_({.cmd = static_cast<TuyaCommandType>(0x03), .payload = {status}});
    return;
  }
  this->lp_session_.reset_status();
  // A queued CMD02 is updated in place, keeping the order on the wire (version 3: the empty mode query).
  for (auto &queued : this->command_queue_) {
    if (queued.cmd == LP_NETWORK_CMD) {
      if (!queued.payload.empty())
        queued.payload[0] = status;
      return;
    }
  }
  if (this->lp_v3_) {
    this->send_empty_command_(LP_NETWORK_CMD);
  } else {
    this->send_command_({.cmd = LP_NETWORK_CMD, .payload = {status}});
  }
}

void TuyaUART::send_low_power_time_(uint8_t command) {
  // Local calendar time (version 0: CMD06, version 3: CMD1C).
  std::array<uint8_t, 8> payload{};
#ifdef USE_TIME
  if (this->time_id_ != nullptr) {
    const auto now = this->time_id_->now();
    if (now.is_valid() && now.year >= 2000 && now.year <= 2255) {
      const uint8_t weekday = static_cast<uint8_t>(now.day_of_week == 1 ? 7 : now.day_of_week - 1);
      payload = {
          1,
          static_cast<uint8_t>(now.year - 2000),
          static_cast<uint8_t>(now.month),
          static_cast<uint8_t>(now.day_of_month),
          static_cast<uint8_t>(now.hour),
          static_cast<uint8_t>(now.minute),
          static_cast<uint8_t>(now.second),
          weekday,
      };
    }
  }
#endif
  if (payload[0] == 0) {
    ESP_LOGW(LP_TAG, "MCU asked for the time before it was known");
  } else {
    ESP_LOGD(LP_TAG, "Sending local time to the MCU (CMD%02X)", command);
  }
  this->send_low_power_direct_(command, payload.data(), payload.size());
}

void TuyaUART::finish_low_power_sync_() {
  if (this->init_state_ == TuyaInitState::INIT_DONE)
    return;
  this->init_state_ = TuyaInitState::INIT_DONE;
  this->mark_initialized_();
  this->mcu_ready_callback_.call();  // Once: init_state_ stays INIT_DONE
  ESP_LOGI(LP_TAG, "MCU status synchronized");
}

void TuyaUART::handle_low_power_command_(uint8_t command, uint8_t version, const uint8_t *data, size_t len) {
  switch (command) {
    case 0x00:  // Heartbeat; 00 only right after the MCU started.
      if (len != 1 || data[0] != 0)
        return;
      if (!this->lp_session_.has_product()) {
        // During a recovery the frame handler sends CMD01 directly; a queued one would resync a second time.
        if (!this->lp_product_retry_active_)
          this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
      } else {
        this->lp_session_.reset_status();
        this->lp_network_status_ = 0xFF;
        this->lp_v3_mode_acked_ = false;  // A restarted version-3 MCU needs the working-mode handshake again
        this->send_low_power_network_();
        this->send_empty_command_(static_cast<TuyaCommandType>(0x13));
      }
      return;

    case 0x01: {  // Product info
      TuyaLowPowerProductInfo incoming{};
      ESP_LOGD(LP_TAG, "MCU product info: %.*s", static_cast<int>(len > 200 ? 200 : len),
               reinterpret_cast<const char *>(data));
      if (!incoming.parse(data, len)) {
        // Like the original module firmware, ask again after 500 ms, a bounded number of times.
        this->lp_session_.command_timeout();
        this->begin_low_power_product_retry_(App.get_loop_component_start_time());
        ESP_LOGW(LP_TAG, "Malformed product info; asking again");
        return;
      }
      // Shown by dump_config like the product info of the standard protocol
      this->product_ = std::make_unique<char[]>(len + 1);
      std::memcpy(this->product_.get(), data, len);
      const bool had_product = this->lp_session_.has_product();
      this->lp_product_ = incoming;
      this->lp_v3_mode_acked_ = false;
      this->lp_v3_pushes_ = false;
      if (this->lp_v3_ != (version == 3)) {
        this->lp_v3_ = version == 3;
        ESP_LOGI(LP_TAG, "MCU frames with protocol version %u: status %s, writes %s", version,
                 this->lp_v3_ ? LOG_STR_LITERAL("CMD08/CMD07") : LOG_STR_LITERAL("CMD05"),
                 this->lp_v3_ ? LOG_STR_LITERAL("CMD06") : LOG_STR_LITERAL("CMD09"));
      }
      this->lp_product_retry_active_ = false;
      this->status_clear_warning();
      if (had_product) {
        // A repeated product info does not prove that the known state is still current.
        this->lp_session_.reset_status();
        this->lp_network_status_ = 0xFF;
      }
      this->lp_session_.product_received();
      // A valid product info ends the quarantine after an ACK timeout; the full status is still needed.
      if (this->lp_wire_.faulted())
        this->lp_wire_.recovered();
      if (this->init_state_ != TuyaInitState::INIT_DONE)
        this->init_state_ = TuyaInitState::INIT_DATAPOINT;
      ESP_LOGI(LP_TAG, "MCU PID %s version %s", this->lp_product_.pid.data(), this->lp_product_.version.data());
      this->send_low_power_network_();
      return;
    }

    case 0x02:  // Network state ACK; for a version-3 MCU the working-mode answer.
      if (this->lp_v3_) {
        // Like the original module firmware, report the network state (CMD03) and ask for the status (CMD08).
        this->lp_v3_mode_acked_ = true;
        if (this->lp_network_status_ != 0xFF)
          this->send_low_power_direct_(0x03, {this->lp_network_status_});
        ESP_LOGD(LP_TAG, "Querying MCU status with CMD08");
        this->lp_v3_last_poll_ms_ = App.get_loop_component_start_time();
        this->send_low_power_direct_(0x08, {});
      }
      return;

    case 0x03:  // Network state ACK of a version-3 MCU
      return;

    case 0x25:  // The original module firmware acknowledges this notice with an empty CMD25.
      if (this->lp_v3_ && len == 0)
        this->send_low_power_direct_(0x25, {});
      return;

    case 0x07: {  // Status report of a version-3 MCU
      if (!this->lp_v3_ || !valid_datapoints(data, len)) {
        ESP_LOGW(LP_TAG, "Rejecting malformed CMD07 report");
        return;
      }
      this->lp_session_.observe_report(App.get_loop_component_start_time());
      if (!this->lp_v3_pushes_ && this->lp_session_.synchronized() &&
          App.get_loop_component_start_time() - this->lp_v3_last_poll_ms_ > 3000) {
        this->lp_v3_pushes_ = true;
        ESP_LOGI(LP_TAG, "MCU reports changes on its own; status polling stopped");
      }
      this->handle_datapoints_(data, len);
      // A version-3 MCU sends one complete report, so that report is the full status.
      if (!this->lp_session_.synchronized()) {
        this->lp_session_.mark_synchronized();
        this->finish_low_power_sync_();
      }
      return;
    }

    case 0x05: {  // Datapoint reports of a version-0 MCU; each datapoint is acknowledged on its own.
      if (len == 2 && data[0] == 0)
        return;  // An ACK, not a report
      // Check the whole list first; a malformed frame gets no ACK at all.
      if (!valid_datapoints(data, len)) {
        ESP_LOGW(LP_TAG, "Rejecting malformed CMD05 report");
        return;
      }
      for (size_t offset = 0; offset < len; offset += 4 + encode_uint16(data[offset + 2], data[offset + 3]))
        this->send_low_power_direct_(0x05, {0x00, data[offset]});
      this->lp_session_.observe_report(App.get_loop_component_start_time());
      this->handle_datapoints_(data, len);
      return;
    }

    case 0x06:  // Local time query
      if (len == 0)
        this->send_low_power_time_(0x06);
      return;

    case 0x1C:  // Local time query of a version-3 MCU
      if (this->lp_v3_ && len == 0)
        this->send_low_power_time_(0x1C);
      return;

    case 0x09:  // Write ACK; the new value is only confirmed by the following report.
      if (len == 0)
        this->lp_session_.write_ack(App.get_loop_component_start_time());
      return;

    case LP_V0_SLEEP_CMD:  // A version-0 MCU asks whether the module can sleep: always busy.
      if (len == 1 && data[0] == 0)
        this->send_low_power_direct_(LP_V0_SLEEP_CMD, {0x00, 0x01});
      return;

    case LP_V3_SLEEP_CMD:  // A version-3 MCU asks to sleep: always busy.
      if (this->lp_v3_ && len >= 1 && data[0] == LP_V3_SLEEP_REQUEST)
        this->send_low_power_direct_(LP_V3_SLEEP_CMD, {LP_V3_SLEEP_REQUEST, LP_V3_BUSY_NETWORK});
      return;

    default:
      ESP_LOGD(LP_TAG, "Unhandled MCU CMD 0x%02X v%u len %u", command, version, static_cast<unsigned>(len));
      return;
  }
}

}  // namespace esphome::tuya

#endif  // USE_TUYA_LOW_POWER
