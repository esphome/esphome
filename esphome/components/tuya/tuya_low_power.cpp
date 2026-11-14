#include "tuya.h"

#ifdef USE_TUYA_LOW_POWER

#include <cctype>
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
// The HC32 sleeps within a second of its last byte; after this long without one it is woken before a command.
static constexpr uint32_t LP_MCU_SILENT_MS = 200;
// sm=2: quiet time before the module offers sleep with 0x91, and how long it waits for the answer.
// The original T3-U firmware uses the same 5 s.
static constexpr uint32_t LP_V3_SLEEP_IDLE_MS = 5000;
static constexpr uint32_t LP_V3_OFFER_TIMEOUT_MS = 1000;
static constexpr uint8_t LP_V3_SLEEP_CMD = 0x91;
static constexpr uint8_t LP_V3_SLEEP_OFFER = 0x00;
static constexpr uint8_t LP_V3_SLEEP_WAKE = 0x01;
static constexpr uint8_t LP_V3_SLEEP_REQUEST = 0x02;
// Reply flags to the MCU's own sleep request, as in the original firmware.
static constexpr uint8_t LP_V3_BUSY_NETWORK = 0x02;
static constexpr uint8_t LP_V3_BUSY_UPGRADE = 0x04;

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
  this->lp_uart_rx_errors_.reset(this->low_power_uart_rx_error_count_());
  if (this->mcu_wake_pin_ == nullptr && this->lp_host_ != nullptr &&
      (this->mcu_wake_pin_ = this->lp_host_->mcu_wake_output()) != nullptr) {
    // Learned from an earlier product info: a sleeping MCU answers CMD01 only after a wake pulse.
    this->lp_mcu_wake_from_host_ = true;
  }
  if (this->mcu_wake_pin_ != nullptr) {
    this->mcu_wake_pin_->setup();
    this->mcu_wake_pin_->digital_write(false);  // Inactive logical level.
  }
  if (this->lp_host_ != nullptr) {
    this->lp_host_->require_external_mcu();
    this->update_low_power_sleep_lock_();
  }
  // The original WXU probes CMD01 at each candidate baud, without a heartbeat.
  // With a fixed rate the existing queued CMD01 request is sufficient.
  if (this->auto_baud_) {
    this->lp_autobaud_.start(this->parent_->get_baud_rate());
  } else {
    this->send_low_power_product_query_(false);
  }
  this->set_interval(LP_NETWORK_STATUS_INTERVAL_ID, 1000, [this] { this->send_low_power_network_(); });
}

uint32_t Tuya::low_power_uart_rx_error_count_() const {
  return this->lp_host_ != nullptr ? this->lp_host_->uart_rx_error_count() : 0;
}

bool Tuya::check_low_power_uart_rx_errors_() {
  if (this->parent_ == nullptr)
    return false;
  const uint32_t count = this->low_power_uart_rx_error_count_();
  if (!this->lp_uart_rx_errors_.observe(count, this->auto_baud_ && this->lp_autobaud_.searching()))
    return false;

  // An already-validated MCU report or ACK could have disappeared from RX.
  // Never grant CMD38/standby using a potentially stale 17-DP snapshot.
  ESP_LOGW(LP_TAG, "UART RX corruption/loss (counter=%u): MCU identity and full status required again",
           static_cast<unsigned>(count));
  this->cancel_low_power_preflight_(true);
  this->cancel_low_power_standby_();
  this->rx_message_.clear();
  this->lp_session_.command_timeout();
  this->lp_wire_.transport_fault();  // Do not replay an ambiguous CMD09/ACK.
  this->expected_response_.reset();
  if (!this->command_queue_.empty()) {
    ESP_LOGW(LP_TAG, "Discarded %u queued MCU commands after UART RX loss",
             static_cast<unsigned>(this->command_queue_.size()));
    this->command_queue_.clear();
  }
  this->lp_network_status_ = 0xFF;
  this->invalidate_low_power_mode_epoch_();
  // The direct CMD01 retry is permitted while the wire is quarantined;
  // a verified product response will unlock it and request full status.
  this->lp_product_retry_.begin(App.get_loop_component_start_time());
  this->update_low_power_sleep_lock_();
  return true;
}

void Tuya::loop_low_power_() {
  if (this->check_low_power_uart_rx_errors_())
    this->discard_available();
  this->poll_low_power_sleep_preflight_();
  if (this->lp_standby_.requested()) {
    auto *host = this->lp_host_;
    if (host == nullptr || !host->standby_requested() || host->ota_busy() || !host->standby_supported() ||
        !host->standby_may_quiesce() || !this->low_power_ready_for_sleep_()) {
      // Withdraw the MCU's logical-sleep signal on any new protocol,
      // network, OTA or application work, before standby has been entered.
      this->cancel_low_power_standby_();
    }
  }
  if (this->parent_ == nullptr || !this->parent_->is_connected()) {
    if (!this->lp_standby_.recovery_required()) {
      this->lp_standby_.transport_fault();
      this->lp_session_.command_timeout();
      this->lp_wire_.transport_fault();
      this->lp_network_status_ = 0xFF;
      ESP_LOGE(LP_TAG, "MCU UART unhealthy: MCU transactions cancelled, standby blocked until recovery");
    }
    if (this->lp_host_ != nullptr)
      this->lp_host_->request_standby(false);
    this->update_low_power_sleep_lock_();
    return;  // No further CMD09 or autobaud probes on a failed transport.
  }
  if (this->auto_baud_) {
    if (this->lp_autobaud_.searching())
      this->poll_low_power_autobaud_();
    if (!this->lp_autobaud_.locked())
      return;  // Never send queued MCU writes/status before CMD01 lock.
  }
  this->poll_low_power_product_recovery_();
  if (this->lp_session_.expire(App.get_loop_component_start_time())) {
    ESP_LOGW(LP_TAG, "Low-power status/write report timeout; requesting state resynchronization");
    this->lp_network_status_ = 0xFF;
  }
  this->update_low_power_sleep_lock_();
  this->process_low_power_queue_();
  this->poll_low_power_v3_status_();
  this->poll_low_power_v3_sleep_();
  this->poll_low_power_update_probe_();
  this->poll_low_power_mcu_update_();
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

void Tuya::dump_low_power_config_() {
  ESP_LOGCONFIG(LP_TAG, "  Protocol: low_power");
  ESP_LOGCONFIG(LP_TAG, "  Auto baud: %s", YESNO(this->auto_baud_));
  if (this->auto_baud_ && this->lp_autobaud_.locked()) {
    ESP_LOGCONFIG(LP_TAG, "  Detected baud: %u", static_cast<unsigned>(this->lp_autobaud_.baud()));
  }
  // The standard "Product" line stays empty: low_power keeps CMD01 in a fixed buffer.
  ESP_LOGCONFIG(LP_TAG, "  MCU product: '%s'", this->lp_product_text_.data());
}

void Tuya::on_low_power_rx_byte_() {
  this->lp_v3_idle_since_ms_ = App.get_loop_component_start_time();
  // An incoming byte before CMD38 consent invalidates a pending network
  // preflight. The MCU must see a busy response, not a delayed grant.
  if (this->lp_cmd38_pending_) {
    this->cancel_low_power_preflight_(true);
    this->lp_cmd38_cooldown_until_ = App.get_loop_component_start_time() + 500;
  }
  // The first arriving byte, not only a fully checksummed frame, must
  // preempt a provisional CMD38 -> CMD12 -> CPU standby transaction.
  // Otherwise partial/noisy UART RX can race the final PM admission check.
  if (this->lp_standby_.requested())
    this->cancel_low_power_standby_();
}

void Tuya::preempt_low_power_sleep_() {
  if (this->lp_cmd38_pending_)
    this->cancel_low_power_preflight_(true);
  if (this->lp_standby_.requested())
    this->cancel_low_power_standby_();
}

void Tuya::handle_low_power_frame_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len) {
  if (this->auto_baud_ && this->lp_autobaud_.searching()) {
    // validate_message_ already checked the full checksum. Keep the scanner
    // independent so bad/wrong frames cannot unlock the boot state machine.
    if (!this->lp_autobaud_.accept_frame(this->rx_message_.data(), this->rx_message_.size(),
                                         App.get_loop_component_start_time())) {
      // Show what the MCU sent, so a reply the scan does not recognize can be diagnosed.
      char hex[3 * 12 + 1];
      const size_t shown = len < 12 ? len : 12;
      for (size_t i = 0; i < shown; ++i)
        snprintf(hex + 3 * i, 4, "%02X ", buffer[i]);
      hex[3 * shown] = '\0';
      ESP_LOGD(LP_TAG, "Baud scan ignored CMD 0x%02X v%u len %u: %s| %.*s", command, version,
               static_cast<unsigned>(len), hex, static_cast<int>(len > 120 ? 120 : len),
               reinterpret_cast<const char *>(buffer));
      return;
    }
    ESP_LOGI(LP_TAG, "Tuya low_power baud locked: %u", static_cast<unsigned>(this->lp_autobaud_.baud()));
  }

  TuyaCommandType command_type = (TuyaCommandType) command;

  // CMD02/CMD09 are only acknowledged by an *empty* reply. CMD05 reports
  // never consume a queued command, and any unsolicited matching command ID
  // must not release an unrelated transaction.
  // A matching reply delivered after its deadline but before the periodic
  // timeout sweep must not complete an expired transaction.
  if (this->lp_wire_.overdue(App.get_loop_component_start_time()) && this->lp_wire_.accepts(command, len)) {
    ESP_LOGW(LP_TAG, "Discarding overdue MCU ACK CMD 0x%02X; recovery required", command);
    return;
  }
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
  this->preempt_low_power_sleep_();
  this->last_command_timestamp_ = millis();
  if (!this->begin_low_power_command_(command))
    return;
  ESP_LOGV(LP_TAG, "Sending MCU CMD=0x%02X len=%u", static_cast<uint8_t>(command.cmd),
           static_cast<unsigned>(command.payload.size()));
  // Same frame as the standard path: 55 AA, version 0, command, length, payload, checksum.
  this->send_low_power_direct_(static_cast<uint8_t>(command.cmd), command.payload);
}

bool Tuya::begin_low_power_command_(const TuyaCommand &command) {
  const auto cmd = static_cast<uint8_t>(command.cmd);
  if (cmd == 0x01 || cmd == 0x02 || cmd == 0x09) {
    if (!this->lp_wire_.begin(cmd, App.get_loop_component_start_time())) {
      ESP_LOGE(LP_TAG, "Refusing overlapping/ambiguous low-power MCU wire transaction");
      this->lp_session_.command_timeout();
      this->lp_wire_.transport_fault();
      return false;
    }
    this->expected_response_ = command.cmd;
  }
  if (cmd == 0x02 && command.payload.size() == 1 && command.payload[0] == 0x04 && !this->lp_v3_)
    this->lp_session_.begin_full_status(App.get_loop_component_start_time());
  if (cmd == 0x06 && this->lp_v3_)
    this->lp_v3_last_poll_ms_ =
        App.get_loop_component_start_time();  // Its CMD07 echo is a reply, not a report of its own.
  if (cmd == 0x09 && command.payload.size() >= 4) {
    const auto &p = command.payload;
    const size_t size = (static_cast<size_t>(p[2]) << 8) | p[3];
    if (p.size() == size + 4)
      this->lp_session_.begin_write(p[0], p[1], p.data() + 4, size, App.get_loop_component_start_time());
  }
  this->update_low_power_sleep_lock_();
  return true;
}

void Tuya::send_low_power_datapoint_(uint8_t datapoint_id, TuyaDatapointType datapoint_type,
                                     const std::vector<uint8_t> &data) {
  if (this->lp_standby_.requested() && this->lp_session_.ready() && this->lp_wire_.idle() &&
      this->lp_host_ != nullptr && this->lp_host_->standby_keeps_network()) {
    // A write from the network while the MCU sleeps under its standby consent: end the consent,
    // wake the MCU, then send CMD12/00 and the write once the wake pulse is over.
    const bool compensate = this->lp_standby_.force_cancel();
    this->lp_host_->request_standby(false);
    this->begin_mcu_wake_pulse_();
    if (compensate) {
      if (this->lp_mcu_wake_pulse_active_) {
        this->lp_awake_notification_pending_ = true;
      } else {
        this->send_low_power_awake_notification_();
      }
    }
    this->update_low_power_sleep_lock_();
  }
  if (!this->lp_session_.ready() || !this->lp_wire_.idle() || this->lp_standby_.requested() ||
      this->lp_mcu_update_ != McuUpdate::MCU_UPDATE_IDLE) {
    ESP_LOGW(LP_TAG, "Refusing DP%u write before MCU synchronization or while another transaction owns UART",
             static_cast<unsigned>(datapoint_id));
    return;
  }
  // The original MCU DP102 handler blindly reads four bytes even for
  // RAW1 and can consume checksum/buffer memory. Reject unsafe writes
  // BEFORE serialization, regardless of whether a higher-level entity
  // calls a forced setter or sends an unknown datapoint.
  if (!TuyaLowPowerSleepWindow::valid_datapoint(datapoint_id, static_cast<uint8_t>(datapoint_type), data.data(),
                                                data.size())) {
    ESP_LOGE(LP_TAG, "Refusing malformed MCU DP%u (type=%u, len=%u)", datapoint_id,
             static_cast<unsigned>(datapoint_type), static_cast<unsigned>(data.size()));
    return;
  }
  if (datapoint_id == TuyaLowPowerSleepWindow::ENABLE_DP || datapoint_id == TuyaLowPowerSleepWindow::WINDOW_DP) {
    this->lp_sleep_window_.on_write(datapoint_id);
    this->invalidate_low_power_mode_epoch_();
  }
  std::vector<uint8_t> buffer;
  buffer.reserve(data.size() + 4);
  buffer.push_back(datapoint_id);
  buffer.push_back(static_cast<uint8_t>(datapoint_type));
  buffer.push_back(data.size() >> 8);
  buffer.push_back(data.size() >> 0);
  buffer.insert(buffer.end(), data.begin(), data.end());
  // The HC32 low-power firmware 1.3.1 expects DP writes as CMD09; version-3 MCUs take the standard CMD06.
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

// Ends a finished wake pulse and sends what waited for it. True while the pulse still runs:
// nothing may go to the MCU before it has woken.
bool Tuya::mcu_wake_pulse_running_() {
  if (!this->lp_mcu_wake_pulse_active_)
    return false;
  if (static_cast<int32_t>(millis() - this->lp_mcu_wake_pulse_until_) < 0)
    return true;
  this->mcu_wake_pin_->digital_write(false);
  this->lp_mcu_wake_pulse_active_ = false;
  if (this->lp_awake_notification_pending_) {
    this->lp_awake_notification_pending_ = false;
    this->send_low_power_awake_frame_();
  }
  return false;
}

void Tuya::send_low_power_product_query_(bool direct_probe) {
  // The original WXU calls its MCU-wake helper *before* CMD01. We pulse
  // only when a wake GPIO is explicitly configured and its polarity verified.
  // The GPIO configuration supports inverted pins for active-low wiring.
  if (this->mcu_wake_pin_ != nullptr) {
    this->mcu_wake_pin_->digital_write(true);
    this->set_timeout(LP_PREPROBE_WAKE_TIMEOUT_ID, this->lp_product_.mcu_wake_pulse_ms, [this, direct_probe]() {
      this->mcu_wake_pin_->digital_write(false);
      if (direct_probe) {
        this->send_low_power_direct_(0x01, {});
      } else {
        this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
      }
    });
    return;
  }
  if (direct_probe) {
    this->send_low_power_direct_(0x01, {});
  } else {
    this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
  }
}

void Tuya::poll_low_power_autobaud_() {
  const uint32_t now = App.get_loop_component_start_time();
  if (!this->lp_autobaud_.probe_due(now)) {
    return;
  }

  const uint32_t baud = this->lp_autobaud_.baud();
  // Bytes received at a wrong candidate rate fail the frame checksum and are dropped.
  if (this->parent_ != nullptr) {
    this->parent_->set_baud_rate(baud);
    this->parent_->load_settings(false);
  }
  if (this->parent_ == nullptr || !this->parent_->is_connected()) {
    ESP_LOGE(LP_TAG, "Unable to configure MCU UART for %u baud", static_cast<unsigned>(baud));
    this->lp_autobaud_.stop();
    this->init_failed_ = true;
    this->status_set_warning(LOG_STR("MCU UART baud rate"));
    return;
  }
  this->rx_message_.clear();  // Drop a partial frame from the previous rate.
  this->lp_autobaud_.probe_sent(App.get_loop_component_start_time());
  this->send_low_power_product_query_(true);
  if (this->lp_autobaud_.attempts() % 8 == 0) {
    ESP_LOGW(LP_TAG, "Still scanning MCU UART (9600/115200), %u probes",
             static_cast<unsigned>(this->lp_autobaud_.attempts()));
  } else {
    ESP_LOGD(LP_TAG, "Autobaud CMD01 probe at %u", static_cast<unsigned>(baud));
  }
}

void Tuya::invalidate_low_power_mode_epoch_() {
  // The HC32's 'sm' is computed by its RTC task, not by the WXU. A write or
  // changed report for DP101/102 makes the last product-info selection stale.
  this->lp_sleep_window_.invalidate_product();
  if (this->lp_session_.has_product()) {
    this->lp_mode_requery_pending_ = true;
    this->lp_mode_requery_after_ms_ = App.get_loop_component_start_time() + 500;
  }
  this->update_low_power_sleep_lock_();
}

void Tuya::poll_low_power_product_recovery_() {
  if (this->init_failed_) {
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();
  // Never inject an autonomous CMD01 during a report burst or a write ACK.
  // After an ambiguous ACK timeout, queued writes must not block the
  // direct CMD01 recovery probe used to re-establish the MCU identity.
  const bool idle = !this->lp_session_.busy() && !this->lp_wire_.active() &&
                    (this->lp_wire_.faulted() || this->command_queue_.empty()) &&
                    !this->expected_response_.has_value() && this->rx_message_.empty();
  if (this->lp_product_retry_.active()) {
    if (!this->lp_product_retry_.due(now) || !idle) {
      return;
    }
    if (this->lp_product_retry_.exhausted()) {
      this->init_failed_ = true;
      this->lp_mode_requery_pending_ = false;
      this->lp_session_.command_timeout();
      ESP_LOGE(LP_TAG, "No valid MCU product info after the bounded CMD01 retries");
      this->status_set_warning(LOG_STR("Malformed MCU product info"));
      this->lp_product_retry_.success();
      return;
    }
    this->lp_product_retry_.query_sent(now);
    this->send_low_power_product_query_(true);
    return;
  }

  if (this->lp_mode_requery_pending_ && idle && static_cast<int32_t>(now - this->lp_mode_requery_after_ms_) >= 0) {
    this->lp_mode_requery_pending_ = false;
    // Unlike original cached sm, our MCU component can revalidate a new
    // product epoch without resetting the platform.
    this->send_low_power_product_query_(false);
  }
}

bool Tuya::low_power_ready_for_sleep_(bool in_cmd38_handler) const {
  // A validated CMD38 frame is still present in rx_message_ until the
  // parser callback returns. Only that specific frame can be disregarded.
  const bool guard_elapsed =
      static_cast<int32_t>(App.get_loop_component_start_time() - this->lp_wake_guard_until_) >= 0;
  const bool baud_locked = !this->auto_baud_ || this->lp_autobaud_.locked();
  // The DP101/DP102 sleep window exists only on the HC32 firmware (version 0 frames).
  return this->lp_session_.ready() && this->lp_wire_.idle() &&
         (this->lp_v3_ || this->lp_sleep_window_.standby_mode_stable()) && !this->lp_product_retry_.active() &&
         guard_elapsed && baud_locked && this->parent_ != nullptr && this->parent_->is_connected() &&
         this->lp_uart_rx_errors_.unchanged(this->low_power_uart_rx_error_count_()) &&
         !this->lp_standby_.recovery_required() && this->command_queue_.empty() && !this->lp_mcu_wake_pulse_active_ &&
         !this->lp_awake_notification_pending_ && !this->expected_response_.has_value() &&
         this->lp_mcu_update_ == McuUpdate::MCU_UPDATE_IDLE && (in_cmd38_handler || this->rx_message_.empty());
}

void Tuya::update_low_power_sleep_lock_() {
  if (this->lp_host_ != nullptr)
    this->lp_host_->set_mcu_synchronized(this->low_power_ready_for_sleep_());
}

bool Tuya::request_low_power_refresh() {
  if (this->protocol_ != TuyaProtocol::TUYA_PROTOCOL_LOW_POWER || !this->lp_session_.ready() ||
      !this->lp_wire_.idle() || this->lp_standby_.requested() || this->expected_response_.has_value())
    return false;
  if (this->lp_v3_) {
    // A version-3 MCU answers the standard status query; DP106 is the HC32's refresh trigger.
    this->lp_v3_last_poll_ms_ = App.get_loop_component_start_time();
    this->send_low_power_direct_(0x08, {});
    return true;
  }
  this->send_datapoint_command_(106, TuyaDatapointType::BOOLEAN, {0x01});
  return true;
}

bool Tuya::low_power_probe_mcu_update(uint32_t image_size) {
  if (this->protocol_ != TuyaProtocol::TUYA_PROTOCOL_LOW_POWER || !this->lp_session_.has_product()) {
    ESP_LOGW(LP_TAG, "MCU update probe: MCU not identified yet");
    return false;
  }
  this->wake_low_power_mcu_();
  this->lp_update_probe_size_ = image_size;
  this->lp_update_probe_ = UpdateProbe::UPDATE_PROBE_PENDING;
  return true;
}

// Wakes the MCU for a command of our own; when it sleeps under its standby consent, the consent ends first.
void Tuya::wake_low_power_mcu_() {
  if (this->lp_standby_.requested() && this->lp_host_ != nullptr) {
    const bool compensate = this->lp_standby_.force_cancel();
    this->lp_host_->request_standby(false);
    this->begin_mcu_wake_pulse_();
    if (compensate) {
      if (this->lp_mcu_wake_pulse_active_) {
        this->lp_awake_notification_pending_ = true;
      } else {
        this->send_low_power_awake_notification_();
      }
    }
  } else {
    this->begin_mcu_wake_pulse_();
  }
  this->update_low_power_sleep_lock_();
}

bool Tuya::low_power_update_mcu(const uint8_t *image, size_t size) {
  if (this->protocol_ != TuyaProtocol::TUYA_PROTOCOL_LOW_POWER || !this->lp_session_.has_product() ||
      image == nullptr || size == 0) {
    ESP_LOGW(LP_TAG, "MCU update: MCU not identified yet or no image");
    return false;
  }
  if (this->lp_mcu_update_ != McuUpdate::MCU_UPDATE_IDLE) {
    ESP_LOGW(LP_TAG, "MCU update: already running");
    return false;
  }
  this->lp_mcu_update_image_ = image;
  this->lp_mcu_update_size_ = size;
  this->lp_mcu_update_offset_ = 0;
  this->lp_mcu_update_retries_ = 0;
  this->lp_mcu_update_percent_ = 0;
  this->lp_mcu_update_ = McuUpdate::MCU_UPDATE_START_PENDING;
  this->wake_low_power_mcu_();
  ESP_LOGI(LP_TAG, "MCU update: starting, %u bytes", static_cast<unsigned>(size));
  return true;
}

void Tuya::fail_low_power_mcu_update_(const char *reason) {
  // The MCU installs nothing without a complete image with a valid checksum; it keeps running its firmware.
  ESP_LOGE(LP_TAG, "MCU update failed at %u of %u bytes: %s; the MCU keeps its firmware",
           static_cast<unsigned>(this->lp_mcu_update_offset_), static_cast<unsigned>(this->lp_mcu_update_size_),
           reason);
  this->lp_mcu_update_ = McuUpdate::MCU_UPDATE_IDLE;
  this->update_low_power_sleep_lock_();
}

void Tuya::send_low_power_update_packet_() {
  const uint32_t offset = this->lp_mcu_update_offset_;
  const uint32_t rest = this->lp_mcu_update_size_ - offset;
  this->lp_mcu_update_chunk_ = rest < this->lp_mcu_update_packet_ ? rest : this->lp_mcu_update_packet_;
  std::vector<uint8_t> payload;
  payload.reserve(4 + this->lp_mcu_update_chunk_);
  payload.push_back(offset >> 24);
  payload.push_back(offset >> 16);
  payload.push_back(offset >> 8);
  payload.push_back(offset);
  payload.insert(payload.end(), this->lp_mcu_update_image_ + offset,
                 this->lp_mcu_update_image_ + offset + this->lp_mcu_update_chunk_);
  this->send_low_power_direct_(this->low_power_update_data_cmd_(), payload);
  this->lp_mcu_update_sent_ms_ = App.get_loop_component_start_time();
  this->lp_mcu_update_ =
      this->lp_mcu_update_chunk_ == 0 ? McuUpdate::MCU_UPDATE_FINAL_SENT : McuUpdate::MCU_UPDATE_PACKET_SENT;
}

void Tuya::poll_low_power_mcu_update_() {
  const uint32_t now = App.get_loop_component_start_time();
  switch (this->lp_mcu_update_) {
    case McuUpdate::MCU_UPDATE_START_PENDING: {
      if (this->lp_mcu_wake_pulse_active_ || this->lp_awake_notification_pending_ || !this->lp_wire_.idle())
        return;
      const uint32_t size = this->lp_mcu_update_size_;
      this->send_low_power_direct_(this->low_power_update_start_cmd_(),
                                   {static_cast<uint8_t>(size >> 24), static_cast<uint8_t>(size >> 16),
                                    static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size)});
      this->lp_mcu_update_sent_ms_ = now;
      this->lp_mcu_update_ = McuUpdate::MCU_UPDATE_START_SENT;
      return;
    }
    case McuUpdate::MCU_UPDATE_START_SENT:
      if (now - this->lp_mcu_update_sent_ms_ > 3000)
        this->fail_low_power_mcu_update_("no answer to the start");
      return;
    case McuUpdate::MCU_UPDATE_ERASE_WAIT:
      // The erase command has no answer and blocks the MCU for the whole erase, so wait before packets.
      if (now - this->lp_mcu_update_sent_ms_ < LOW_POWER_UPDATE_ERASE_MS)
        return;
      this->lp_mcu_update_ = McuUpdate::MCU_UPDATE_SEND_PACKET;
      return;
    case McuUpdate::MCU_UPDATE_SEND_PACKET:
      if (this->lp_mcu_wake_pulse_active_)
        return;
      this->send_low_power_update_packet_();
      return;
    case McuUpdate::MCU_UPDATE_PACKET_SENT:
      if (now - this->lp_mcu_update_sent_ms_ < 2000)
        return;
      if (++this->lp_mcu_update_retries_ > 3) {
        this->fail_low_power_mcu_update_("packet not acknowledged");
        return;
      }
      ESP_LOGW(LP_TAG, "MCU update: no acknowledgement at %u, sending again",
               static_cast<unsigned>(this->lp_mcu_update_offset_));
      this->begin_mcu_wake_pulse_();  // The MCU may have fallen asleep.
      this->lp_mcu_update_ = McuUpdate::MCU_UPDATE_SEND_PACKET;
      return;
    case McuUpdate::MCU_UPDATE_FINAL_SENT:
      // A good image makes the MCU restart without an answer; an acknowledgement means its check failed.
      if (now - this->lp_mcu_update_sent_ms_ < 10000)
        return;
      ESP_LOGI(LP_TAG, "MCU update: transfer complete, the MCU installs the image and restarts; restarting the module");
      this->lp_mcu_update_ = McuUpdate::MCU_UPDATE_IDLE;
      App.safe_reboot();
      return;
    default:
      return;
  }
}

// Returns true when the frame belonged to the running update.
bool Tuya::handle_low_power_mcu_update_(uint8_t command, const uint8_t *data, size_t len) {
  if (this->lp_mcu_update_ == McuUpdate::MCU_UPDATE_START_SENT && command == this->low_power_update_start_cmd_()) {
    // Packet size: 0 = 256, 1 = 512, 2 = 1024 bytes
    const uint8_t code = len >= 1 ? data[0] : 0;
    this->lp_mcu_update_packet_ = code == 2 ? 1024 : code == 1 ? 512 : 256;
    ESP_LOGI(LP_TAG, "MCU update: MCU ready, packets of %u bytes", this->lp_mcu_update_packet_);
    if (this->lp_v3_) {
      // The version-3 start command already erased the staging area.
      this->lp_mcu_update_ = McuUpdate::MCU_UPDATE_SEND_PACKET;
    } else {
      // Erase the staging area first; flash keeps old bits otherwise and the checksum fails.
      ESP_LOGI(LP_TAG, "MCU update: erasing the MCU staging area");
      this->send_low_power_direct_(LOW_POWER_UPDATE_ERASE_CMD, {0x00});
      this->lp_mcu_update_sent_ms_ = App.get_loop_component_start_time();
      this->lp_mcu_update_ = McuUpdate::MCU_UPDATE_ERASE_WAIT;
    }
    return true;
  }
  if (command != this->low_power_update_data_cmd_() || len != 0)
    return false;
  if (this->lp_mcu_update_ == McuUpdate::MCU_UPDATE_PACKET_SENT) {
    this->lp_mcu_update_offset_ += this->lp_mcu_update_chunk_;
    this->lp_mcu_update_retries_ = 0;
    const uint8_t percent = static_cast<uint8_t>(100ull * this->lp_mcu_update_offset_ / this->lp_mcu_update_size_);
    if (percent / 10 != this->lp_mcu_update_percent_ / 10) {
      ESP_LOGI(LP_TAG, "MCU update: %u%%", percent);
    }
    this->lp_mcu_update_percent_ = percent;
    // After the last packet an empty one at offset = size ends the transfer.
    this->lp_mcu_update_ = McuUpdate::MCU_UPDATE_SEND_PACKET;
    return true;
  }
  if (this->lp_mcu_update_ == McuUpdate::MCU_UPDATE_FINAL_SENT) {
    this->fail_low_power_mcu_update_("the MCU rejected the image checksum");
    return true;
  }
  return false;
}

// Sends a pending probe once the MCU is awake, and reports a missing answer.
void Tuya::poll_low_power_update_probe_() {
  if (this->lp_update_probe_ == UpdateProbe::UPDATE_PROBE_PENDING) {
    if (this->lp_mcu_wake_pulse_active_ || this->lp_awake_notification_pending_ || !this->lp_wire_.idle())
      return;
    const uint32_t size = this->lp_update_probe_size_;
    const uint8_t cmd = this->low_power_update_start_cmd_();
    this->send_low_power_direct_(cmd, {static_cast<uint8_t>(size >> 24), static_cast<uint8_t>(size >> 16),
                                       static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size)});
    this->lp_update_probe_sent_ms_ = App.get_loop_component_start_time();
    this->lp_update_probe_ = UpdateProbe::UPDATE_PROBE_SENT;
    ESP_LOGI(LP_TAG, "MCU update probe: sent CMD%02X with image size %u (start only, no data)", cmd,
             static_cast<unsigned>(size));
    return;
  }
  if (this->lp_update_probe_ == UpdateProbe::UPDATE_PROBE_SENT &&
      App.get_loop_component_start_time() - this->lp_update_probe_sent_ms_ > 3000) {
    this->lp_update_probe_ = UpdateProbe::UPDATE_PROBE_IDLE;
    ESP_LOGW(LP_TAG, "MCU update probe: no answer to CMD%02X within 3 s", this->low_power_update_start_cmd_());
  }
}

void Tuya::send_low_power_awake_notification_() {
  if (this->lp_v3_ && this->mcu_wake_pin_ != nullptr) {
    // A version-3 MCU sleeps after its consent and listens to the UART only after a wake pulse.
    this->begin_mcu_wake_pulse_();
    if (this->lp_mcu_wake_pulse_active_) {
      this->lp_awake_notification_pending_ = true;
      this->lp_wake_guard_until_ = App.get_loop_component_start_time() + 5000;
      this->update_low_power_sleep_lock_();
      return;
    }
  }
  this->send_low_power_awake_frame_();
}

void Tuya::send_low_power_awake_frame_() {
  // This is a Tuya MCU command, not a platform wake operation.
  if (this->parent_ == nullptr || !this->parent_->is_connected()) {
    this->lp_standby_.transport_fault();
  } else {
    if (this->lp_v3_) {
      this->send_low_power_direct_(LP_V3_SLEEP_CMD, {LP_V3_SLEEP_WAKE, 0x01});
      // The MCU pushes its own changes. Only after a fault is its state read again; that report ends the recovery.
      if (this->lp_standby_.recovery_required()) {
        this->lp_v3_last_poll_ms_ = App.get_loop_component_start_time();
        this->send_low_power_direct_(0x08, {});
      }
    } else {
      this->send_low_power_direct_(0x12, {0x00});
    }
    if (this->flush() != uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS)
      this->lp_standby_.transport_fault();
  }
  this->lp_wake_guard_until_ = App.get_loop_component_start_time() + 5000;
  this->update_low_power_sleep_lock_();
}

void Tuya::configure_low_power_wake_lines_() {
  if (this->lp_wake_lines_configured_ || this->lp_host_ == nullptr)
    return;
  this->lp_wake_lines_configured_ = true;
  // The product info names both wake lines, so the configuration does not have to.
  const bool create_output = this->mcu_wake_pin_ == nullptr || this->lp_mcu_wake_from_host_;
  this->lp_wake_input_ =
      this->lp_host_->configure_wake_lines(this->lp_product_.wake_gpio, this->lp_product_.mcu_wake_gpio,
                                           this->lp_product_.mcu_wake_active_high, create_output);
  if (create_output && (this->mcu_wake_pin_ = this->lp_host_->mcu_wake_output()) != nullptr) {
    this->lp_mcu_wake_from_host_ = true;
    this->mcu_wake_pin_->setup();
    this->mcu_wake_pin_->digital_write(false);
  }
}

bool Tuya::low_power_v3_sleep_supported_() const {
  auto *host = this->lp_host_;
  return this->lp_v3_ && this->lp_product_.sleep_mode == 2 && host != nullptr && host->standby_supported() &&
         this->lp_wake_input_ >= 0 && host->standby_wake_pin() == this->lp_wake_input_ &&
         host->standby_requires_external_sync() && host->standby_may_quiesce();
}

void Tuya::poll_low_power_v3_sleep_() {
  if (!this->lp_v3_ || this->lp_product_.sleep_mode != 2)
    return;
  const uint32_t now = App.get_loop_component_start_time();
  if (this->lp_v3_offer_pending_) {
    if (static_cast<int32_t>(now - this->lp_v3_offer_deadline_ms_) >= 0) {
      this->lp_v3_offer_pending_ = false;
      this->lp_v3_idle_since_ms_ = now;
      ESP_LOGD(LP_TAG, "MCU did not answer the sleep offer");
    }
    return;
  }
  if (this->lp_standby_.requested() || now - this->lp_v3_idle_since_ms_ < LP_V3_SLEEP_IDLE_MS)
    return;
  auto *host = this->lp_host_;
  const LogString *blocked = nullptr;
  bool link_busy = false;  // The usual short pause after a wake: not worth a DEBUG line.
  if (host == nullptr || !host->standby_supported()) {
    blocked = LOG_STR("no standby controller");
  } else if (this->lp_wake_input_ < 0 || host->standby_wake_pin() != this->lp_wake_input_) {
    blocked = LOG_STR("wake input differs from the MCU's");
  } else if (!host->standby_requires_external_sync()) {
    blocked = LOG_STR("require_external_sync is off");
  } else if (!host->standby_may_quiesce()) {
    blocked = LOG_STR("services busy");
  } else if (host->ota_busy()) {
    blocked = LOG_STR("update running");
  } else if (!this->low_power_ready_for_sleep_() || this->available() != 0) {
    link_busy = true;
  }
  if (link_busy) {
    ESP_LOGV(LP_TAG, "No sleep offer: MCU link not idle (ready %d, wire %d, queue %u, guard %d)",
             this->lp_session_.ready(), this->lp_wire_.idle(), static_cast<unsigned>(this->command_queue_.size()),
             static_cast<int32_t>(now - this->lp_wake_guard_until_) >= 0);
    return;
  }
  if (blocked != this->lp_v3_sleep_blocked_) {
    this->lp_v3_sleep_blocked_ = blocked;
    if (blocked != nullptr) {
      ESP_LOGD(LP_TAG, "No sleep offer: %s (blockers 0x%04X)", LOG_STR_ARG(blocked),
               static_cast<unsigned>(host != nullptr ? host->standby_blockers() : 0));
    } else {
      ESP_LOGD(LP_TAG, "Sleep offers to the MCU start");
    }
  }
  if (blocked != nullptr)
    return;
  ESP_LOGV(LP_TAG, "Offering sleep to the MCU");
  this->send_low_power_direct_(LP_V3_SLEEP_CMD, {LP_V3_SLEEP_OFFER});
  this->lp_v3_offer_pending_ = true;
  this->lp_v3_offer_deadline_ms_ = now + LP_V3_OFFER_TIMEOUT_MS;
}

void Tuya::handle_low_power_v3_sleep_(const uint8_t *data, size_t len) {
  if (!this->lp_v3_ || len < 1)
    return;
  auto *host = this->lp_host_;
  switch (data[0]) {
    case LP_V3_SLEEP_OFFER: {  // The MCU's answer to our offer: 01 = sleep, 00 = stay awake.
      if (!this->lp_v3_offer_pending_ || len < 2)
        return;
      this->lp_v3_offer_pending_ = false;
      this->lp_v3_idle_since_ms_ = App.get_loop_component_start_time();
      if (data[1] != this->lp_v3_last_answer_) {
        this->lp_v3_last_answer_ = data[1];
        if (data[1] != 0x01) {
          ESP_LOGD(LP_TAG, "MCU declines sleep (answer %u)", data[1]);
        }
      }
      if (data[1] != 0x01)
        return;
      if (!this->low_power_v3_sleep_supported_() || host->ota_busy() ||
          !this->lp_standby_.accept_offer(this->low_power_ready_for_sleep_(true), this->available() == 0, true)) {
        // The MCU already sleeps and expects the module to sleep too.
        this->send_low_power_awake_notification_();
        return;
      }
      ESP_LOGV(LP_TAG, "MCU agreed to sleep");
      host->request_standby(true);
      this->update_low_power_sleep_lock_();
      return;
    }
    case LP_V3_SLEEP_WAKE:  // The MCU's answer to our wake notice.
      return;
    case LP_V3_SLEEP_REQUEST: {  // The MCU asks to sleep; 00 grants it.
      if (this->lp_standby_.requested()) {
        this->send_low_power_direct_(LP_V3_SLEEP_CMD, {LP_V3_SLEEP_REQUEST, 0x00});
        return;
      }
      uint8_t flags = 0;
      if (host != nullptr && host->ota_busy())
        flags |= LP_V3_BUSY_UPGRADE;
      if (flags == 0 &&
          (!this->low_power_v3_sleep_supported_() ||
           !this->lp_standby_.accept_offer(this->low_power_ready_for_sleep_(true), this->available() == 0, true)))
        flags |= LP_V3_BUSY_NETWORK;
      this->send_low_power_direct_(LP_V3_SLEEP_CMD, {LP_V3_SLEEP_REQUEST, flags});
      if (flags != 0)
        return;
      if (this->flush() != uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS) {
        this->lp_standby_.transport_fault();
        this->update_low_power_sleep_lock_();
        return;
      }
      ESP_LOGV(LP_TAG, "MCU asked to sleep");
      host->request_standby(true);
      this->update_low_power_sleep_lock_();
      return;
    }
    default:
      return;
  }
}

void Tuya::cancel_low_power_preflight_(bool notify_mcu) {
  if (!this->lp_cmd38_pending_)
    return;
  this->lp_cmd38_pending_ = false;
  if (this->lp_host_ != nullptr)
    this->lp_host_->request_preflight(false);
  if (notify_mcu)
    this->send_low_power_direct_(0x38, {0x00, 0x01});
}

void Tuya::poll_low_power_sleep_preflight_() {
  if (!this->lp_cmd38_pending_)
    return;
  auto *host = this->lp_host_;
  if (host == nullptr || !host->standby_supported() || !host->preflight_requested() ||
      static_cast<int32_t>(App.get_loop_component_start_time() - this->lp_cmd38_deadline_ms_) >= 0 ||
      host->ota_busy() || !host->standby_may_quiesce() || !this->low_power_ready_for_sleep_()) {
    this->cancel_low_power_preflight_(true);
    this->lp_cmd38_cooldown_until_ = App.get_loop_component_start_time() + 500;
    return;
  }
  if (!host->standby_services_quiesced())
    return;
  if (this->available() != 0 || this->flush() != uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS ||
      !this->lp_standby_.accept_request(this->lp_product_.sleep_mode == 1, true, true, true)) {
    this->cancel_low_power_preflight_(true);
    return;
  }
  // Never grant CMD38 while network owners still have live sockets, OTA,
  // mDNS callbacks or a WLAN wakelock. The MCU receives its ACK only NOW.
  this->lp_cmd38_pending_ = false;
  this->send_low_power_direct_(0x38, {0x00, 0x00});
  if (this->flush() == uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS) {
    host->request_standby(true);
  } else {
    this->lp_standby_.transport_fault();
    this->update_low_power_sleep_lock_();
  }
  host->request_preflight(false);
}

void Tuya::cancel_low_power_standby_() {
  const bool compensate = this->lp_standby_.force_cancel();
  if (this->lp_host_ != nullptr)
    this->lp_host_->request_standby(false);
  if (compensate)
    this->send_low_power_awake_notification_();
  this->update_low_power_sleep_lock_();
}

bool Tuya::notify_low_power_transition(bool sleeping) {
  if (this->protocol_ != TuyaProtocol::TUYA_PROTOCOL_LOW_POWER || !this->lp_session_.has_product())
    return false;
  if (sleeping) {
    // The generic coordinator's scheduler/wake votes are intentionally
    // FALSE during prepare: requiring can_enter_standby() here deadlocks.
    if (!this->lp_standby_.needs_notification() || !this->low_power_ready_for_sleep_() || this->available() != 0 ||
        this->flush() != uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS)
      return false;
    this->send_low_power_direct_(0x12, {0x01});
    if (this->flush() != uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS) {
      this->lp_standby_.transport_fault();
      this->update_low_power_sleep_lock_();
      return false;
    }
    this->lp_standby_.notification_sent();
    this->update_low_power_sleep_lock_();
    return true;
  }

  if (!this->lp_standby_.force_cancel())
    return false;
  this->send_low_power_awake_notification_();
  return true;
}

bool Tuya::low_power_prepare_for_standby() {
  if (this->lp_host_ == nullptr || !this->lp_standby_.requested() ||
      (this->lp_product_.sleep_mode != 1 && this->lp_product_.sleep_mode != 2) || !this->low_power_ready_for_sleep_() ||
      this->available() != 0)
    return false;
  if (this->lp_standby_.needs_notification()) {
    // Deliberately postpone platform PM; CMD12/01 was transmitted first.
    // standby_cancelled() consumes the intentional first deferral.
    (void) this->notify_low_power_transition(true);
    return false;
  }
  return this->lp_standby_.ready_for_pm() && this->flush() == uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;
}

void Tuya::low_power_standby_cancelled() {
  const bool compensate = this->lp_standby_.cancelled();
  if (!this->lp_standby_.requested() && this->lp_host_ != nullptr)
    this->lp_host_->request_standby(false);
  if (compensate)
    this->send_low_power_awake_notification_();
  this->update_low_power_sleep_lock_();
}

void Tuya::low_power_standby_resumed(bool expected_source, bool wake_gpio) {
  const bool keep = this->lp_host_ != nullptr && this->lp_host_->standby_keeps_network();
  // A version-3 MCU is told about every wake that ends its consent (91 01 01 after a wake pulse),
  // not only about one it caused itself; its state is then read again.
  const bool notify_wake = wake_gpio || (this->lp_v3_ && !keep);
  const auto action = this->lp_standby_.resumed(expected_source, notify_wake, keep);
  if (action == TuyaLowPowerStandbyState::ResumeAction::KEEP_CONSENT) {
    // The MCU did not wake us and still sleeps: no CMD12/00, no wake guard, sleep again once idle.
    this->lp_host_->request_standby(true);
    this->update_low_power_sleep_lock_();
    return;
  }
  this->lp_wake_guard_until_ = App.get_loop_component_start_time() + 5000;
  // A retained GPIO wake may receive CMD12/00 only if we previously sent
  // CMD12/01. Timer-only wake must not impersonate the original GPIO path.
  if (action == TuyaLowPowerStandbyState::ResumeAction::SEND_CMD12_WAKE) {
    this->send_low_power_awake_notification_();
  } else if (action == TuyaLowPowerStandbyState::ResumeAction::REQUIRE_RESYNC) {
    this->lp_session_.command_timeout();
    ESP_LOGE(LP_TAG, "Unexpected/timer-only MCU resume: withholding CMD12/00 and further standby");
    // A version-3 MCU sends no boot heartbeat: wake it and read its state to recover.
    if (this->lp_v3_)
      this->send_low_power_awake_notification_();
  }
  this->update_low_power_sleep_lock_();
}

void Tuya::process_low_power_queue_() {
  const uint32_t now = App.get_loop_component_start_time();
  // The byte and command timestamps of tuya.cpp come from App.get_loop_component_start_time(), so compare them with it.
  const uint32_t wire_now = millis();
  if (static_cast<uint32_t>(wire_now - this->last_rx_char_timestamp_) > LP_RX_TIMEOUT_MS)
    this->rx_message_.clear();
  if (this->mcu_wake_pulse_running_())
    return;

  // Standard Tuya's 300 ms retry logic is incompatible with the HC32's
  // delayed CMD05 report. ACK and report have separate timeouts.
  // The session gate, not an optional ESPHome command field, owns the
  // physical in-flight transaction. It can time out after a reset or an
  // asynchronous recovery callback has already cleared expected_response_.
  const bool product_in_flight = this->lp_wire_.kind() == TuyaLowPowerWireTransactions::Kind::PRODUCT;
  if (this->lp_wire_.expire(now)) {
    const bool product_query = product_in_flight;
    this->expected_response_.reset();
    if (!this->command_queue_.empty())
      this->command_queue_.erase(this->command_queue_.begin());
    this->lp_session_.command_timeout();
    this->lp_network_status_ = 0xFF;
    this->invalidate_low_power_mode_epoch_();
    // Recover with the direct CMD01 probe (it wakes the MCU first): the queued re-query of the
    // invalidate helper would wait behind the quarantined queue for good.
    this->lp_product_retry_.begin(now);
    ESP_LOGW(LP_TAG, "MCU command ACK timeout (product=%s); quarantining writes until full resync",
             YESNO(product_query));
    // A late ACK cannot be distinguished from one for a subsequent write.
    // Do not silently replay queued commands after an ambiguous timeout.
    const size_t dropped = this->command_queue_.size();
    this->command_queue_.clear();
    if (dropped > 0) {
      ESP_LOGW(LP_TAG, "Discarded %u queued MCU commands after ambiguous ACK timeout", static_cast<unsigned>(dropped));
    }
  }

  if (this->lp_session_.busy() || this->lp_wire_.active() || this->lp_wire_.faulted() ||
      this->expected_response_.has_value() || !this->rx_message_.empty() || this->command_queue_.empty() ||
      static_cast<uint32_t>(wire_now - this->last_command_timestamp_) <= LP_COMMAND_DELAY_MS)
    return;
  if (!this->lp_session_.has_product() && this->command_queue_.front().cmd != TuyaCommandType::PRODUCT_QUERY)
    return;
  // A low-power MCU sleeps whenever it is idle and listens only after a pulse on its wake line, so
  // wake it before a command when it has been silent; the command goes out once the pulse is over.
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

void Tuya::send_low_power_direct_(uint8_t command, const std::vector<uint8_t> &payload) {
  // ACKs and time replies must not wait behind a pending CMD09 write.
  if (payload.size() > 65535)
    return;
  this->lp_v3_idle_since_ms_ = App.get_loop_component_start_time();
  const uint8_t header[] = {
      0x55, 0xAA, 0x00, command, static_cast<uint8_t>(payload.size() >> 8), static_cast<uint8_t>(payload.size()),
  };
  uint8_t checksum = 0;
  for (uint8_t b : header)
    checksum += b;
  for (uint8_t b : payload)
    checksum += b;
  this->write_array(header, sizeof(header));
  if (!payload.empty())
    this->write_array(payload.data(), payload.size());
  this->write_byte(checksum);
}

void Tuya::send_low_power_network_() {
  if (!this->lp_session_.has_product())
    return;
  // An intentional, pending Wi-Fi teardown for standby is not a new logical
  // Tuya network epoch: defer CMD02 until abort/resume, when the station
  // is restored. External/unplanned link loss remains visible as before.
  if (this->lp_host_ != nullptr && this->lp_host_->network_quiescing() &&
      (this->lp_host_->standby_requested() || this->lp_host_->preflight_requested()))
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
    // Standard Tuya framing: the network state is CMD03 (the original T3-U firmware sends it on
    // every change); CMD02 is the working-mode query. The MCU reports changes on its own only
    // after it was told that the module is online.
    this->send_low_power_direct_(0x03, {status});
    return;
  }
  this->lp_session_.reset_status();
  if (status == 0x04)
    this->lp_wake_guard_until_ =
        App.get_loop_component_start_time() + (this->lp_product_.sleep_mode == 0 ? 10000 : 5000);
  this->update_low_power_sleep_lock_();
  this->send_command_({.cmd = static_cast<TuyaCommandType>(0x02), .payload = {status}});
}

void Tuya::send_low_power_time_(uint8_t command) {
  // Local calendar time (HC32: CMD06, version-3 MCU: CMD1C). No duplicate timezone/DST adjustment.
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

void Tuya::handle_low_power_command_(uint8_t command, uint8_t version, const uint8_t *data, size_t len) {
  (void) version;
  if (this->lp_mcu_update_ != McuUpdate::MCU_UPDATE_IDLE && this->handle_low_power_mcu_update_(command, data, len))
    return;
  if (this->lp_update_probe_ == UpdateProbe::UPDATE_PROBE_SENT && command == this->low_power_update_start_cmd_()) {
    this->lp_update_probe_ = UpdateProbe::UPDATE_PROBE_IDLE;
    char hex[3 * 8 + 1] = "";
    const size_t shown = len < 8 ? len : 8;
    for (size_t i = 0; i < shown; ++i)
      snprintf(hex + 3 * i, 4, "%02X ", data[i]);
    // Standard Tuya answer: one byte, the packet size (0 = 256, 1 = 512, 2 = 1024 bytes).
    ESP_LOGI(LP_TAG, "MCU update probe: answer CMD%02X len %u: %s", command, static_cast<unsigned>(len), hex);
    return;
  }
  // Once CMD38 is provisionally accepted, any new MCU traffic invalidates
  // the quiet interval. Do not race a report burst against CPU standby.
  if (command != 0x38 && command != LP_V3_SLEEP_CMD && this->lp_standby_.requested())
    this->cancel_low_power_standby_();
  switch (command) {
    case 0x00:  // First MCU heartbeat after reset is 00; subsequent is 01.
      if (len != 1 || data[0] != 0)
        return;
      this->lp_standby_.reset_on_mcu_boot();
      // The HC32 may have a new RTC sleep-window marker (sm), including
      // after a PA3 pulse or local UI change; force a fresh product epoch.
      if (this->lp_session_.has_product()) {
        this->invalidate_low_power_mode_epoch_();
        this->lp_sleep_window_.reset_reports();
      }
      if (this->lp_host_ != nullptr)
        this->lp_host_->request_standby(false);
      if (!this->lp_session_.has_product()) {
        this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
      } else {
        this->lp_session_.reset_status();
        this->lp_network_status_ = 0xFF;
        this->update_low_power_sleep_lock_();
        this->send_low_power_network_();
        this->send_empty_command_(static_cast<TuyaCommandType>(0x13));
      }
      return;

    case 0x01: {  // Product-info response.
      TuyaLowPowerProductInfo incoming{};
      // The descriptor differs between MCU firmware versions; show it once for diagnosis.
      ESP_LOGD(LP_TAG, "MCU product info: %.*s", static_cast<int>(len > 200 ? 200 : len),
               reinterpret_cast<const char *>(data));
      if (!incoming.parse(data, len)) {
        // Original WXU calls CMD01 again after 500 ms for missing descriptors,
        // allocation/JSON failures or absent p/v. Retry only malformed data;
        // never treat its failed initialization marker as a working MCU.
        this->invalidate_low_power_mode_epoch_();
        this->lp_session_.command_timeout();
        this->lp_product_retry_.begin(App.get_loop_component_start_time());
        ESP_LOGW(LP_TAG, "Malformed CMD01 product info: retrying at 500ms intervals (bounded)");
        return;
      }
      // parse() limits len to 512, so the raw descriptor fits our fixed
      // diagnostics buffer without an extra allocation and without an
      // implementation-dependent nothrow-new/abort path.
      std::memcpy(this->lp_product_text_.data(), data, len);
      this->lp_product_text_[len] = '\0';
      const bool had_product = this->lp_session_.has_product();
      this->lp_product_ = std::move(incoming);
      this->lp_v3_mode_acked_ = false;
      this->lp_v3_pushes_ = false;
      if (this->lp_v3_ != (version == 3)) {
        this->lp_v3_ = version == 3;
        ESP_LOGI(LP_TAG, "MCU frames with protocol version %u: status %s, writes %s", version,
                 this->lp_v3_ ? LOG_STR_LITERAL("CMD08/CMD07") : LOG_STR_LITERAL("CMD05 burst"),
                 this->lp_v3_ ? LOG_STR_LITERAL("CMD06") : LOG_STR_LITERAL("CMD09"));
      }
      this->lp_product_retry_.success();
      this->lp_mode_requery_pending_ = false;
      this->lp_sleep_window_.product_validated();
      if (had_product) {
        // HC32 RTC/DP101/DP102 may have changed the requested PM mode.
        // Subsequent CMD01 never proves MCU status is still synchronized.
        this->lp_session_.reset_status();
        this->lp_sleep_window_.reset_reports();
        this->lp_network_status_ = 0xFF;
      }
      this->lp_session_.product_received();
      // Revalidated product identity clears the wire quarantine, but not
      // the MCU full-status gate (17 new CMD05 reports are still required).
      if (this->lp_wire_.faulted())
        this->lp_wire_.recovered();
      this->init_state_ = TuyaInitState::INIT_DATAPOINT;
      ESP_LOGI(LP_TAG, "MCU PID %s version %s wk=%d mwk=%d sm=%d", this->lp_product_.pid.data(),
               this->lp_product_.version.data(), this->lp_product_.wake_gpio, this->lp_product_.mcu_wake_gpio,
               this->lp_product_.sleep_mode);
      this->configure_low_power_wake_lines_();
      this->send_low_power_network_();
      return;
    }

    case 0x02:  // Network-state ACK; generic queue handler clears expected CMD.
      if (this->lp_v3_) {
        // For a version-3 MCU CMD02 is the working-mode query. Then, like the original T3-U
        // firmware, report the network state (CMD03) and ask for the status (CMD08, answered by CMD07).
        this->lp_v3_mode_acked_ = true;
        if (this->lp_network_status_ != 0xFF)
          this->send_low_power_direct_(0x03, {this->lp_network_status_});
        ESP_LOGD(LP_TAG, "Querying MCU status with CMD08");
        this->lp_v3_last_poll_ms_ = App.get_loop_component_start_time();
        this->send_low_power_direct_(0x08, {});
      }
      return;

    case 0x03:  // ACK of the network state (version-3 MCU).
      return;

    case LP_V3_SLEEP_CMD:  // Sleep handshake of a version-3 MCU with sm=2.
      this->handle_low_power_v3_sleep_(data, len);
      return;

    case 0x25:  // The original T3-U firmware acknowledges this MCU notice with an empty CMD25.
      if (this->lp_v3_ && len == 0)
        this->send_low_power_direct_(0x25, {});
      return;

    case 0x07: {  // Status report of a version-3 MCU (standard Tuya framing).
      if (!this->lp_v3_ || len < 5)
        return;
      if (!this->lp_v3_pushes_ && this->lp_session_.synchronized() &&
          App.get_loop_component_start_time() - this->lp_v3_last_poll_ms_ > 3000) {
        this->lp_v3_pushes_ = true;
        ESP_LOGI(LP_TAG, "MCU reports changes on its own; status polling stopped");
      }
      size_t offset = 0;
      while (offset + 4 <= len) {
        const size_t size = (static_cast<size_t>(data[offset + 2]) << 8) | data[offset + 3];
        if (size > len - offset - 4) {
          ESP_LOGW(LP_TAG, "Rejecting malformed MCU CMD07 report");
          return;
        }
        this->lp_session_.observe_dp(data[offset], data[offset + 1], data + offset + 4, size);
        offset += 4 + size;
      }
      this->handle_datapoints_(data, len);
      // A version-3 MCU sends no boot heartbeat; a complete report proves the link works again.
      this->lp_standby_.recovered();
      if (!this->lp_session_.synchronized()) {
        this->lp_session_.mark_synchronized();
        this->init_state_ = TuyaInitState::INIT_DONE;
        if (!this->lp_initialized_callback_called_) {
          this->lp_initialized_callback_called_ = true;
          this->initialized_callback_.call();
        }
        ESP_LOGI(LP_TAG, "MCU status synchronized (version-3 MCU)");
      }
      if (this->lp_v3_pushes_)
        this->notify_mcu_ready_();
      this->update_low_power_sleep_lock_();
      return;
    }

    case 0x05: {  // MCU datapoint reports; acknowledge each DP separately.
      if (len == 2 && data[0] == 0)
        return;  // ACK, not a report: don't ACK an ACK.
      if (len < 5)
        return;
      // First validate the complete list; never partially ACK malformed frames.
      size_t offset = 0;
      while (offset < len) {
        if (len - offset < 4)
          return;
        const size_t size = (static_cast<size_t>(data[offset + 2]) << 8) | data[offset + 3];
        const uint8_t type = data[offset + 1];
        if (size > len - offset - 4 || type > 5 || ((type == 1 || type == 4) && size != 1) ||
            (type == 2 && size != 4) || (type == 1 && data[offset + 4] > 1) ||
            !TuyaLowPowerSleepWindow::valid_datapoint(data[offset], type, data + offset + 4, size)) {
          ESP_LOGW(LP_TAG, "Rejecting malformed MCU DP report; no partial CMD05 acknowledgements");
          return;
        }
        offset += 4 + size;
      }
      offset = 0;
      while (offset < len) {
        const size_t size = (static_cast<size_t>(data[offset + 2]) << 8) | data[offset + 3];
        this->send_low_power_direct_(0x05, {0x00, data[offset]});
        const bool previous_product_epoch = this->lp_sleep_window_.product_epoch_valid();
        this->lp_sleep_window_.observe_report(data[offset], data[offset + 1], data + offset + 4, size);
        if (previous_product_epoch && !this->lp_sleep_window_.product_epoch_valid())
          this->invalidate_low_power_mode_epoch_();
        this->lp_session_.observe_dp(data[offset], data[offset + 1], data + offset + 4, size);
        offset += 4 + size;
      }
      this->handle_datapoints_(data, len);
      if (this->init_state_ != TuyaInitState::INIT_DONE && this->lp_session_.synchronized()) {
        this->init_state_ = TuyaInitState::INIT_DONE;
        if (!this->lp_initialized_callback_called_) {
          this->lp_initialized_callback_called_ = true;
          this->initialized_callback_.call();
        }
        ESP_LOGI(LP_TAG, "MCU status synchronized for current product epoch");
      }
      // The HC32 takes commands once its full status is in.
      if (this->lp_session_.synchronized())
        this->notify_mcu_ready_();
      this->update_low_power_sleep_lock_();
      return;
    }

    case 0x06:  // Local time query.
      if (len == 0)
        this->send_low_power_time_(0x06);
      return;

    case 0x1C:  // Local time query of a version-3 MCU (standard Tuya); its weekly program needs the clock.
      if (this->lp_v3_ && len == 0)
        this->send_low_power_time_(0x1C);
      return;

    case 0x09:  // ACK alone never means that a new setpoint was adopted.
      if (len == 0) {
        this->lp_session_.write_ack();
        this->update_low_power_sleep_lock_();
      }
      return;

    case 0x38: {  // MCU asks whether WXU can enter low-power.
      if (len != 1 || data[0] != 0)
        return;
      // Never confuse a repeated sleep request with a completion ACK.
      if (this->lp_cmd38_pending_) {
        // The previous request is still pending. Send exactly one busy
        // response; never replay a second CMD38 ACK for the same transaction.
        this->cancel_low_power_preflight_(true);
        this->lp_cmd38_cooldown_until_ = App.get_loop_component_start_time() + 500;
        return;
      }
      if (this->lp_standby_.requested() ||
          static_cast<int32_t>(App.get_loop_component_start_time() - this->lp_cmd38_cooldown_until_) < 0) {
        this->cancel_low_power_standby_();
        this->send_low_power_direct_(0x38, {0x00, 0x01});
        return;
      }
      uint8_t flags = 0x01;  // Default is *busy*, not a provisional grant.
      auto *host = this->lp_host_;
      const bool ota_veto = host != nullptr && host->ota_busy();
      const bool supported = host != nullptr && host->standby_supported() && host->standby_wake_pin() >= 0 &&
                             host->standby_requires_external_sync() &&
                             host->standby_wake_pin() == this->lp_wake_input_ && this->lp_product_.sleep_mode == 1 &&
                             host->standby_may_quiesce();
      const bool transport_ready = this->low_power_ready_for_sleep_(true) && this->available() == 0;
      if (supported && !ota_veto && transport_ready) {
        if (!host->standby_services_quiesced()) {
          // Start *service* preparation without requesting CPU PM. Do not
          // tell HC32 that WXU accepted sleep before every network owner
          // has actually closed/drained its resources.
          this->lp_cmd38_pending_ = true;
          this->lp_cmd38_deadline_ms_ = App.get_loop_component_start_time() + 1800;
          host->request_preflight(true);
          return;  // A bounded later busy/accept response will be sent.
        }
        const bool accepted = this->lp_standby_.accept_request(true, true, true, true);
        if (accepted) {
          this->send_low_power_direct_(0x38, {0x00, 0x00});
          if (this->flush() == uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS)
            host->request_standby(true);
          else
            this->lp_standby_.transport_fault();
          this->update_low_power_sleep_lock_();
          return;
        }
      }
      if (ota_veto)
        flags |= 0x02;
      this->send_low_power_direct_(0x38, {0x00, flags});
      return;
    }

    default: {
      // Shown at DEBUG while other MCU firmware versions are being mapped.
      char hex[3 * 24 + 1];
      const size_t shown = len < 24 ? len : 24;
      for (size_t i = 0; i < shown; ++i)
        snprintf(hex + 3 * i, 4, "%02X ", data[i]);
      hex[3 * shown] = '\0';
      ESP_LOGD(LP_TAG, "Unhandled MCU CMD 0x%02X v%u len %u: %s", command, version, static_cast<unsigned>(len), hex);
      return;
    }
  }
}

}  // namespace esphome::tuya

#endif  // USE_TUYA_LOW_POWER
