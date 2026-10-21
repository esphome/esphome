#pragma once

#ifdef USE_ESP32

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "esphome/components/ring_buffer/ring_buffer.h"
#include "esphome/components/speaker/speaker.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
#include <esp_gap_bt_api.h>

namespace esphome::a2dp_source {

class A2DPSourceSpeaker final : public Component, public speaker::Speaker {
 public:
  bool set_address(const char *address, bool persist = false);
  void set_slot(uint8_t slot) { this->slot_ = slot; }
  bool clear_address();
  bool bonded() const;
  bool restored_from_flash() const { return this->restored_from_flash_; }
  std::string address_string() const;
  bool pair(const char *address, const char *pin = "", bool require_discovered = true);
  bool select_bond(const char *address);
  bool forget_bond(const char *address);
  const char *forget_status() const;
  bool confirm_pairing(bool approve);
  std::string confirmation_prompt() const;
  bool unpair();
  bool start_discovery();
  void cancel_discovery();
  std::string discovery_results(uint8_t page = 0) const;
  size_t discovery_count() const;
  std::string discovery_device(size_t index) const;
  int bond_count() const;
  std::string bond_list(uint16_t page = 0) const;
  void set_bond_page(uint16_t page) { this->bond_page_.store(page); }
  uint16_t bond_page() const { return this->bond_page_.load(); }
  bool discovery_running() const;
  const char *pairing_status() const;
  void setup() override;
  void loop() override;
  void dump_config() override;

  size_t play(const uint8_t *data, size_t length) override { return this->play(data, length, 0); }
  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
  void start() override;
  void prepare_for_media() override;
  void stop() override;
  void finish() override;
  bool has_buffered_data() const override;
  void set_volume(float volume) override;
  void set_mute_state(bool mute_state) override;
  uint32_t callback_count() const { return this->callback_count_.load(); }
  uint32_t pcm_bytes() const { return this->pcm_bytes_.load(); }
  uint32_t underrun_bytes() const { return this->underrun_bytes_.load(); }
  uint32_t underrun_events() const { return this->underrun_events_.load(); }
  uint32_t max_callback_gap_us() const { return this->max_callback_gap_us_.load(); }
  bool connected() const { return this->connected_.load(); }
  bool is_ready_for_media() const override { return this->connected_.load() && this->avrcp_ready_.load(); }
  bool streaming() const { return this->streaming_.load(); }
  bool requested() const { return this->requested_.load(); }
  bool pairing_in_progress() const { return this->pairing_.load(); }
  bool profile_ready() const { return this->profile_ready_.load(); }
  bool connection_failed_latched() const { return this->connection_failed_latched_.load(); }
  void acknowledge_connection_failure() { this->connection_failed_latched_.store(false); }
  uint32_t connection_failures() const { return this->connection_failure_count_.load(); }
  uint32_t connection_attempts_total() const { return this->connection_attempt_count_.load(); }
  std::string connection_trace() const;
  void clear_connection_trace();
  bool waiting_for_retry() const;
  void reset_connection_sequence();
  bool avrcp_ready() const { return this->avrcp_ready_.load(); }
  int avrcp_volume() const { return this->avrcp_actual_volume_.load(); }
  const char *avrcp_status() const;

 protected:
  static void a2dp_event_(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
  static void avrcp_event_(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);
  static void gap_event_(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
  static int32_t audio_data_(uint8_t *data, int32_t length);
  void process_event_(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
  void process_avrcp_event_(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);
  void process_gap_event_(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
  bool has_address_() const;
  bool is_bonded_(const uint8_t *address) const;
  bool save_address_();
  void reset_connection_state_();
  void abort_connection_(const char *reason);
  void retry_or_abort_connection_(const char *reason);
  void trace_event_(uint8_t code, uint16_t detail = 0);

  struct ConnectionTraceEvent {
    uint32_t ms;
    uint32_t attempt;
    uint16_t detail;
    uint8_t code;
  };
  static constexpr size_t CONNECTION_TRACE_SIZE = 48;
  mutable portMUX_TYPE trace_mux_ = portMUX_INITIALIZER_UNLOCKED;
  ConnectionTraceEvent connection_trace_[CONNECTION_TRACE_SIZE]{};
  size_t connection_trace_next_{0};
  size_t connection_trace_count_{0};

  uint8_t address_[6]{};
  uint8_t previous_address_[6]{};
  uint8_t slot_{0};
  bool restored_from_flash_{false};
  ESPPreferenceObject address_pref_;
  char pin_[17]{};
  std::atomic<bool> pairing_{false};
  std::atomic<bool> pair_authenticated_{false};
  uint32_t pair_deadline_ms_{0};
  enum class PairingStatus : uint8_t { IDLE, CONNECTING, WAITING_FOR_AUTH, WAITING_CONFIRM, PAIRED, FAILED };
  std::atomic<PairingStatus> pairing_status_{PairingStatus::IDLE};
  std::atomic<bool> preempted_{false};
  std::atomic<bool> confirmation_pending_{false};
  uint8_t confirmation_address_[6]{};
  uint32_t confirmation_number_{0};
  std::atomic<bool> remove_bond_pending_{false};
  std::atomic<uint8_t> remove_bond_result_{0};
  uint32_t remove_bond_deadline_ms_{0};
  uint8_t forget_address_[6]{};
  std::atomic<bool> forget_pending_{false};
  std::atomic<uint8_t> forget_result_{0};
  uint32_t forget_deadline_ms_{0};
  enum class ForgetStatus : uint8_t { IDLE, PENDING, REMOVED, FAILED };
  std::atomic<ForgetStatus> forget_status_{ForgetStatus::IDLE};
  std::atomic<uint16_t> bond_page_{0};
  std::unique_ptr<ring_buffer::RingBuffer> pcm_buffer_{};
  std::atomic<bool> profile_ready_{false};
  std::atomic<bool> connect_pending_{false};
  std::atomic<bool> connection_failed_{false};
  std::atomic<bool> connection_failed_latched_{false};
  std::atomic<uint32_t> connection_failure_count_{0};
  std::atomic<uint32_t> connection_attempt_count_{0};
  std::atomic<bool> failure_reported_for_attempt_{false};
  std::atomic<uint8_t> connect_attempts_{0};
  std::atomic<bool> connection_session_active_{false};
  std::atomic<bool> disconnect_pending_{false};
  std::atomic<bool> media_check_pending_{false};
  std::atomic<bool> media_start_pending_{false};
  std::atomic<bool> finish_pending_{false};
  std::atomic<bool> connected_{false};
  std::atomic<bool> acl_connected_{false};
  std::atomic<bool> streaming_{false};
  std::atomic<bool> requested_{false};
  std::atomic<uint32_t> callback_count_{0};
  std::atomic<uint32_t> underrun_bytes_{0};
  std::atomic<uint32_t> underrun_events_{0};
  std::atomic<uint32_t> pcm_bytes_{0};
  std::atomic<uint32_t> max_callback_gap_us_{0};
  std::atomic<bool> muted_{false};
  std::atomic<bool> received_first_data_{false};
  std::atomic<bool> prepared_start_{false};
  std::atomic<bool> avrcp_connected_{false};
  std::atomic<bool> avrcp_ready_{false};
  std::atomic<bool> avrcp_failed_{false};
  std::atomic<int> avrcp_actual_volume_{-1};
  std::atomic<int> avrcp_response_volume_{-1};
  std::atomic<uint8_t> avrcp_target_volume_{0};
  std::atomic<uint8_t> avrcp_last_requested_{255};
  uint8_t avrcp_transaction_{0};
  uint8_t avrcp_attempts_{0};
  std::atomic<bool> avrcp_command_pending_{false};
  uint32_t avrcp_command_deadline_ms_{0};
  uint32_t avrcp_connect_deadline_ms_{0};
  std::atomic<int64_t> previous_callback_us_{0};
  std::atomic<uint32_t> next_connect_attempt_ms_{0};
  uint32_t scanner_wait_started_ms_{0};
  bool scanner_stop_starting_requested_{false};
  uint32_t next_media_check_ms_{0};
};

}  // namespace esphome::a2dp_source

#endif  // USE_ESP32
