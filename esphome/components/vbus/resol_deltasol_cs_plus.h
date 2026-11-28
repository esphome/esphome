#pragma once

#include "esphome/core/defines.h"

#ifdef USE_VBUS_CS_PLUS
#include "vbus.h"
#include "vbus_cs_plus_policy.h"
#include <array>
#include <bitset>

namespace esphome::vbus {

#ifdef USE_VBUS_CS_PLUS_WRITE
enum class SettingWriteStatus : uint8_t {
  SETTING_WRITE_STATUS_IDLE,
  SETTING_WRITE_STATUS_PREFLIGHT,
  SETTING_WRITE_STATUS_VERIFYING,
  SETTING_WRITE_STATUS_SUCCESS,
  SETTING_WRITE_STATUS_UNCHANGED,
  SETTING_WRITE_STATUS_FAILED,
  SETTING_WRITE_STATUS_UNCERTAIN,
  SETTING_WRITE_STATUS_REJECTED
};
#endif

class ResolDeltaSolCSPlus final : public PollingComponent, public VBusListener {
 public:
  ResolDeltaSolCSPlus(VBus *receiver, VBusPolling *polling, VBusTransmitter *transmitter);
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  bool start_scan();
  bool is_scanning() const { return this->phase_ != Phase::PHASE_IDLE; }
  bool has_fresh_snapshot() const;
  bool has_recent_snapshot() const;
  bool telemetry_online() const;
  float snapshot_age_seconds() const;
  bool get_value(uint16_t index, int32_t &value) const;
  uint32_t completed_scans() const { return this->completed_scans_; }
  uint32_t telemetry_count() const { return this->telemetry_count_; }
  uint16_t request_count() const { return this->request_count_; }
  uint16_t received_count() const { return this->received_count_; }
  uint16_t group_count() const { return this->group_count_; }
  uint16_t release_count() const { return this->release_count_; }
  uint32_t elapsed_ms() const { return this->elapsed_ms_; }
  uint32_t maximum_lease_ms() const { return this->maximum_lease_ms_; }
#ifdef USE_VBUS_CS_PLUS_WRITE
  void set_require_write_arm(bool required) { this->require_write_arm_ = required; }
  void arm_writes(bool enabled);
  bool writes_armed() const;
  bool request_setting(uint16_t index, float value);
  bool write_uncertain() const { return this->write_uncertain_; }
  SettingWriteStatus write_status() const { return this->write_status_; }
  uint32_t writes_sent() const { return this->transmitter_->get_write_count(); }
  uint32_t writes_verified() const { return this->writes_verified_; }
  template<typename F> void add_on_setting_result_callback(F &&callback) {
    this->setting_result_callback_.add(std::forward<F>(callback));
  }
#endif
  template<typename F> void add_on_snapshot_callback(F &&callback) {
    this->snapshot_callback_.add(std::forward<F>(callback));
  }

 protected:
  static constexpr size_t COUNT = cs_plus::SETTING_COUNT;
  enum class Phase : uint8_t { PHASE_IDLE, PHASE_GROUP, PHASE_TELEMETRY };
  void handle_message(std::vector<uint8_t> &message) override;
  void on_result_(uint32_t grant, uint16_t command, uint16_t index, uint32_t value, TransactionStatus status);
  void add_read_(uint16_t index);
  void extend_plan_();
  void start_group_();
  void abort_();
  int32_t working_(uint16_t index) const;
  void check_age_();
#ifdef USE_VBUS_CS_PLUS_WRITE
  enum class WritePhase : uint8_t { IDLE, PREFLIGHT, VERIFY };
  void advance_write_();
  void on_write_result_(uint16_t index, uint32_t value, TransactionStatus status);
  void finish_write_(SettingWriteStatus status);
  WritePhase write_phase_{WritePhase::IDLE};
  SettingWriteStatus write_status_{SettingWriteStatus::SETTING_WRITE_STATUS_IDLE};
  uint32_t write_arm_at_{0};
  uint32_t writes_before_{0};
  uint32_t write_started_at_{0};
  uint32_t writes_verified_{0};
  int32_t write_value_{0};
  uint16_t write_index_{0};
  bool require_write_arm_{true};
  bool write_armed_{false};
  bool write_uncertain_{false};
  bool context_refresh_pending_{false};
  bool write_verified_{false};
  bool write_cleanup_{false};
  CallbackManager<void(uint16_t, SettingWriteStatus)> setting_result_callback_;
#endif

  VBus *receiver_;
  VBusPolling *polling_;
  VBusTransmitter *transmitter_;
  std::array<int32_t, COUNT> working_values_{};
  std::array<int32_t, COUNT> snapshot_{};
  std::array<uint16_t, COUNT> plan_{};
  std::bitset<COUNT> planned_;
  std::bitset<COUNT> received_;
  std::bitset<COUNT> snapshot_present_;
  uint32_t snapshot_at_{0};
  uint32_t telemetry_at_{0};
  uint32_t attempt_at_{0};
  uint32_t group_at_{0};
  uint32_t releases_before_group_{0};
  uint32_t releases_at_start_{0};
  uint32_t reads_at_start_{0};
  uint32_t maximum_lease_ms_{0};
  uint32_t elapsed_ms_{0};
  uint32_t completed_scans_{0};
  uint32_t telemetry_count_{0};
  size_t plan_count_{0};
  size_t position_{0};
  uint16_t request_count_{0};
  uint16_t received_count_{0};
  uint16_t group_count_{0};
  uint16_t release_count_{0};
  uint16_t clock_minutes_{0};
  uint8_t remaining_{0};
  uint8_t plan_stage_{0};
  Phase phase_{Phase::PHASE_IDLE};
  bool attempted_{false};
  bool profile_ok_{false};
  bool has_snapshot_{false};
  bool snapshot_retained_{false};
  bool resumed_{false};
  bool stopped_{false};
  bool resume_pending_{false};
  CallbackManager<void()> snapshot_callback_;
};

}  // namespace esphome::vbus
#endif
