#include "tuya.h"

#ifdef USE_TUYA_LOW_POWER

#include <cstring>
#include <utility>

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#ifdef USE_NETWORK
#include "esphome/components/network/util.h"
#endif

namespace esphome::tuya {

static const char *const LP_TAG = "tuya.low_power";
static constexpr uint32_t LP_RX_TIMEOUT_MS = 300;
// A version-3 MCU (firmware 1.0.9) reports changes made at the device only when asked.
static constexpr uint32_t LP_V3_STATUS_POLL_MS = 60000;
static constexpr uint32_t LP_COMMAND_DELAY_MS = 10;
// The MCU sleeps within a second of its last byte; after this long without one it is woken before a command.
static constexpr uint32_t LP_MCU_SILENT_MS = 200;
// After a malformed product info or a lost answer, ask again every 500 ms, at most 5 times.
static constexpr uint32_t LP_PRODUCT_RETRY_MS = 500;
static constexpr uint8_t LP_PRODUCT_RETRIES = 5;
// Sleep requests of the MCU. The module does not sleep, so both are always answered with "busy".
static constexpr uint8_t LP_V0_SLEEP_CMD = 0x38;
static constexpr uint8_t LP_V3_SLEEP_CMD = 0x91;
static constexpr uint8_t LP_V3_SLEEP_REQUEST = 0x02;
static constexpr uint8_t LP_V3_BUSY_NETWORK = 0x02;

// Scheduler ids continue those in tuya.cpp. The network status shares the Wi-Fi status slot
// there, because a hub runs one protocol only.
static constexpr uint32_t LP_NETWORK_STATUS_INTERVAL_ID = 1;
static constexpr uint32_t LP_PREPROBE_WAKE_TIMEOUT_ID = 4;

void Tuya::setup_low_power_() {
  this->init_state_ = TuyaInitState::INIT_PRODUCT;
  if (this->parent_ == nullptr) {
    this->init_failed_ = true;
    this->mark_failed(LOG_STR("MCU UART missing"));
    return;
  }
  if (this->mcu_wake_pin_ != nullptr) {
    this->mcu_wake_pin_->setup();
    this->mcu_wake_pin_->digital_write(false);  // Inactive logical level.
  }
  this->send_low_power_product_query_(false);
  this->set_interval(LP_NETWORK_STATUS_INTERVAL_ID, 1000, [this] { this->send_low_power_network_(); });
}

void Tuya::loop_low_power_() {
  if (!this->parent_->is_connected()) {
    if (!this->lp_session_.faulted()) {
      this->lp_session_.command_timeout();
      this->lp_wire_.transport_fault();
      this->lp_network_status_ = 0xFF;
      ESP_LOGE(LP_TAG, "MCU UART unhealthy: MCU transactions cancelled until recovery");
    }
    return;
  }
  this->poll_low_power_product_recovery_();
  switch (this->lp_session_.update(App.get_loop_component_start_time())) {
    case TuyaLowPowerSession::Event::EVENT_SYNCHRONIZED:
      this->finish_low_power_sync_();
      this->notify_mcu_ready_();  // A version-0 MCU takes commands once its full status is in.
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

void Tuya::poll_low_power_v3_status_() {
  if (!this->lp_v3_ || this->lp_v3_pushes_ || !this->lp_session_.ready() || !this->lp_wire_.idle() ||
      !this->command_queue_.empty() || !this->rx_message_.empty())
    return;
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->lp_v3_last_poll_ms_ < LP_V3_STATUS_POLL_MS)
    return;
  this->lp_v3_last_poll_ms_ = now;
  ESP_LOGV(LP_TAG, "Polling MCU status with CMD08");
  this->send_low_power_direct_(0x08, {});
}

void Tuya::handle_low_power_frame_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len) {
  // CMD01/02/09 are acknowledged by the matching command. A late answer to an expired command must not
  // complete a newer one.
  if (this->lp_wire_.overdue(App.get_loop_component_start_time()) && this->lp_wire_.accepts(command, len)) {
    ESP_LOGW(LP_TAG, "Discarding overdue MCU ACK CMD 0x%02X; recovery required", command);
    return;
  }
  const auto command_type = static_cast<TuyaCommandType>(command);
  if (this->lp_wire_.complete(command, len) && this->expected_response_.has_value() &&
      this->expected_response_ == command_type) {
    this->expected_response_.reset();
    if (!this->command_queue_.empty())
      this->command_queue_.erase(this->command_queue_.begin());
    this->init_retries_ = 0;
  }
  this->handle_low_power_command_(command, version, buffer, len);
}

void Tuya::send_low_power_raw_command_(const TuyaCommand &command) {
  this->last_command_timestamp_ = millis();
  const auto cmd = static_cast<uint8_t>(command.cmd);
  const uint32_t now = App.get_loop_component_start_time();
  if (cmd == 0x01 || cmd == 0x02 || cmd == 0x09) {
    if (!this->lp_wire_.begin(cmd, now)) {
      ESP_LOGE(LP_TAG, "Refusing an overlapping MCU transaction");
      this->lp_session_.command_timeout();
      this->lp_wire_.transport_fault();
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
  this->send_low_power_direct_(cmd, command.payload);
}

void Tuya::send_low_power_datapoint_(uint8_t datapoint_id, TuyaDatapointType datapoint_type,
                                     const std::vector<uint8_t> &data) {
  if (!this->lp_session_.ready() || !this->lp_wire_.idle()) {
    ESP_LOGW(LP_TAG, "Refusing DP%u write before the MCU is in sync or while another transaction runs",
             static_cast<unsigned>(datapoint_id));
    return;
  }
  std::vector<uint8_t> buffer{datapoint_id, static_cast<uint8_t>(datapoint_type),
                              static_cast<uint8_t>(data.size() >> 8), static_cast<uint8_t>(data.size())};
  buffer.insert(buffer.end(), data.begin(), data.end());
  // Version-0 MCUs take writes as CMD09, version-3 MCUs as the standard CMD06.
  this->send_command_(TuyaCommand{.cmd = static_cast<TuyaCommandType>(this->lp_v3_ ? 0x06 : 0x09), .payload = buffer});
}

void Tuya::begin_mcu_wake_pulse_() {
  if (this->mcu_wake_pin_ == nullptr || this->lp_mcu_wake_pulse_active_)
    return;
  this->mcu_wake_pin_->digital_write(true);
  // millis(), not the loop time: the pulse may start late in a loop pass and must last its full length.
  this->lp_mcu_wake_pulse_until_ = millis() + this->lp_product_.mcu_wake_pulse_ms;
  this->lp_mcu_wake_pulse_active_ = true;
}

// Ends a finished wake pulse. True while the pulse still runs: nothing may go to the MCU before it has woken.
bool Tuya::mcu_wake_pulse_running_() {
  if (!this->lp_mcu_wake_pulse_active_)
    return false;
  if (static_cast<int32_t>(millis() - this->lp_mcu_wake_pulse_until_) < 0)
    return true;
  this->mcu_wake_pin_->digital_write(false);
  this->lp_mcu_wake_pulse_active_ = false;
  return false;
}

// direct_probe sends CMD01 at once, outside the quarantined queue, for a retry.
void Tuya::send_low_power_product_query_(bool direct_probe) {
  if (this->mcu_wake_pin_ == nullptr) {
    this->send_product_query_now_(direct_probe);
    return;
  }
  // Like the original module firmware, wake the MCU before CMD01.
  this->mcu_wake_pin_->digital_write(true);
  this->set_timeout(LP_PREPROBE_WAKE_TIMEOUT_ID, this->lp_product_.mcu_wake_pulse_ms, [this, direct_probe]() {
    this->mcu_wake_pin_->digital_write(false);
    this->send_product_query_now_(direct_probe);
  });
}

void Tuya::send_product_query_now_(bool direct_probe) {
  if (direct_probe) {
    this->send_low_power_direct_(0x01, {});
  } else {
    this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
  }
}

void Tuya::begin_low_power_product_retry_(uint32_t now) {
  if (this->lp_product_retries_ != 0)
    return;
  this->lp_product_retries_ = LP_PRODUCT_RETRIES + 1;
  this->lp_product_retry_at_ = now + LP_PRODUCT_RETRY_MS;
}

void Tuya::poll_low_power_product_recovery_() {
  if (this->init_failed_ || this->lp_product_retries_ == 0)
    return;
  const uint32_t now = App.get_loop_component_start_time();
  // Never send CMD01 during a report series or a write. After an ACK timeout the queue is quarantined,
  // so queued commands must not block the retry.
  const bool idle = !this->lp_session_.busy() && !this->lp_wire_.active() &&
                    (this->lp_wire_.faulted() || this->command_queue_.empty()) &&
                    !this->expected_response_.has_value() && this->rx_message_.empty();
  if (static_cast<int32_t>(now - this->lp_product_retry_at_) < 0 || !idle)
    return;
  if (--this->lp_product_retries_ == 0) {
    this->init_failed_ = true;
    this->lp_session_.command_timeout();
    ESP_LOGE(LP_TAG, "No valid MCU product info after %u retries", LP_PRODUCT_RETRIES);
    this->status_set_warning(LOG_STR("Malformed MCU product info"));
    return;
  }
  this->lp_product_retry_at_ = now + LP_PRODUCT_RETRY_MS;
  this->send_low_power_product_query_(true);
}

bool Tuya::request_low_power_refresh() {
  if (!this->is_low_power_() || !this->lp_session_.ready() || !this->lp_wire_.idle() ||
      this->expected_response_.has_value())
    return false;
  if (this->lp_v3_) {
    // A version-3 MCU answers the standard status query.
    this->lp_v3_last_poll_ms_ = App.get_loop_component_start_time();
    this->send_low_power_direct_(0x08, {});
    return true;
  }
  // A version-0 MCU has no status query; DP106 is the refresh request of the HC32 thermostat firmware.
  this->send_datapoint_command_(106, TuyaDatapointType::BOOLEAN, {0x01});
  return true;
}

void Tuya::process_low_power_queue_() {
  const uint32_t now = App.get_loop_component_start_time();
  // The byte and command timestamps of tuya.cpp come from millis(), so compare them with it.
  const uint32_t wire_now = millis();
  if (static_cast<uint32_t>(wire_now - this->last_rx_char_timestamp_) > LP_RX_TIMEOUT_MS)
    this->rx_message_.clear();
  if (this->mcu_wake_pulse_running_())
    return;

  // The standard 300 ms retry does not fit a version-0 MCU, which reports its state some time after the ACK.
  // ACK and report have separate timeouts.
  const bool product_query = this->lp_wire_.active() && this->lp_wire_.command() == 0x01;
  if (this->lp_wire_.expire(now)) {
    this->expected_response_.reset();
    this->lp_session_.command_timeout();
    this->lp_network_status_ = 0xFF;
    // Recover with the direct CMD01 probe; a queued one would wait behind the quarantined queue.
    this->begin_low_power_product_retry_(now);
    ESP_LOGW(LP_TAG, "MCU command ACK timeout (product=%s); writes wait for a full resync", YESNO(product_query));
    // A late ACK cannot be told apart from one for the next write, so do not replay queued commands.
    if (!this->command_queue_.empty()) {
      ESP_LOGW(LP_TAG, "Discarded %u queued MCU commands after the ACK timeout",
               static_cast<unsigned>(this->command_queue_.size()));
      this->command_queue_.clear();
    }
  }

  if (this->lp_session_.busy() || this->lp_wire_.active() || this->lp_wire_.faulted() ||
      this->expected_response_.has_value() || !this->rx_message_.empty() || this->command_queue_.empty() ||
      static_cast<uint32_t>(wire_now - this->last_command_timestamp_) <= LP_COMMAND_DELAY_MS)
    return;
  if (!this->lp_session_.has_product() && this->command_queue_.front().cmd != TuyaCommandType::PRODUCT_QUERY)
    return;
  // A sleeping MCU listens only after a pulse on its wake line, so wake it before a command when it has
  // been silent; the command goes out once the pulse is over.
  if (this->mcu_wake_pin_ != nullptr && wire_now - this->last_rx_char_timestamp_ > LP_MCU_SILENT_MS &&
      wire_now - this->lp_mcu_woken_ms_ > LP_MCU_SILENT_MS) {
    this->lp_mcu_woken_ms_ = wire_now;
    this->begin_mcu_wake_pulse_();
    return;
  }

  this->send_raw_command_(this->command_queue_.front());
  if (!this->expected_response_.has_value())
    this->command_queue_.erase(this->command_queue_.begin());
}

// Sends one frame right away, outside the queue: ACKs and time replies must not wait behind a write.
void Tuya::send_low_power_direct_(uint8_t command, const std::vector<uint8_t> &payload) {
  if (payload.size() > 65535)
    return;
  const uint8_t header[] = {
      0x55, 0xAA, 0x00, command, static_cast<uint8_t>(payload.size() >> 8), static_cast<uint8_t>(payload.size()),
  };
  const uint8_t checksum = sum_bytes(header, sizeof(header)) + sum_bytes(payload.data(), payload.size());
  this->write_array(header, sizeof(header));
  if (!payload.empty())
    this->write_array(payload.data(), payload.size());
  this->write_byte(checksum);
}

void Tuya::send_low_power_network_() {
  if (!this->lp_session_.has_product())
    return;
  uint8_t status = 0x02;
#ifdef USE_NETWORK
  if (network::is_connected())
    status = remote_is_connected() ? 0x04 : 0x03;
#endif
  if (status == this->lp_network_status_)
    return;
  this->lp_network_status_ = status;
  if (this->lp_v3_ && this->lp_v3_mode_acked_) {
    // Standard Tuya framing: the network state is CMD03; CMD02 is the working-mode query. The MCU reports
    // changes on its own only after it was told that the module is online.
    this->send_low_power_direct_(0x03, {status});
    return;
  }
  this->lp_session_.reset_status();
  this->send_command_({.cmd = static_cast<TuyaCommandType>(0x02), .payload = {status}});
}

void Tuya::send_low_power_time_(uint8_t command) {
  // Local calendar time (version 0: CMD06, version 3: CMD1C).
  std::vector<uint8_t> payload(8, 0);
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
  this->send_low_power_direct_(command, payload);
}

void Tuya::finish_low_power_sync_() {
  if (this->init_state_ == TuyaInitState::INIT_DONE)
    return;
  this->init_state_ = TuyaInitState::INIT_DONE;
  this->initialized_callback_.call();
  ESP_LOGI(LP_TAG, "MCU status synchronized");
}

void Tuya::handle_low_power_command_(uint8_t command, uint8_t version, const uint8_t *data, size_t len) {
  switch (command) {
    case 0x00:  // Heartbeat; 00 only right after the MCU started.
      if (len != 1 || data[0] != 0)
        return;
      if (!this->lp_session_.has_product()) {
        this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
      } else {
        this->lp_session_.reset_status();
        this->lp_network_status_ = 0xFF;
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
      this->lp_product_ = std::move(incoming);
      this->lp_v3_mode_acked_ = false;
      this->lp_v3_pushes_ = false;
      if (this->lp_v3_ != (version == 3)) {
        this->lp_v3_ = version == 3;
        ESP_LOGI(LP_TAG, "MCU frames with protocol version %u: status %s, writes %s", version,
                 this->lp_v3_ ? LOG_STR_LITERAL("CMD08/CMD07") : LOG_STR_LITERAL("CMD05"),
                 this->lp_v3_ ? LOG_STR_LITERAL("CMD06") : LOG_STR_LITERAL("CMD09"));
      }
      this->lp_product_retries_ = 0;
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
      if (!this->lp_v3_ || len < 5)
        return;
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
      if (this->lp_v3_pushes_)
        this->notify_mcu_ready_();
      return;
    }

    case 0x05: {  // Datapoint reports of a version-0 MCU; each datapoint is acknowledged on its own.
      if (len == 2 && data[0] == 0)
        return;  // An ACK, not a report
      if (len < 5)
        return;
      // Check the whole list first; a malformed frame gets no ACK at all.
      size_t offset = 0;
      while (offset < len) {
        if (len - offset < 4)
          return;
        const size_t size = encode_uint16(data[offset + 2], data[offset + 3]);
        const uint8_t type = data[offset + 1];
        if (size > len - offset - 4 || type > 5 || ((type == 1 || type == 4) && size != 1) ||
            (type == 2 && size != 4) || (type == 1 && data[offset + 4] > 1)) {
          ESP_LOGW(LP_TAG, "Rejecting malformed CMD05 report");
          return;
        }
        offset += 4 + size;
      }
      for (offset = 0; offset < len; offset += 4 + encode_uint16(data[offset + 2], data[offset + 3]))
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
