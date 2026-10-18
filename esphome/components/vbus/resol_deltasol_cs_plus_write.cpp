#include "resol_deltasol_cs_plus.h"

#ifdef USE_VBUS_CS_PLUS_WRITE
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::vbus {

ESPHOME_LOG_TAG(TAG, "vbus.cs_plus.write");

void ResolDeltaSolCSPlus::arm_writes(bool enabled) {
  this->write_armed_ = enabled && !this->stopped_ && !this->polling_->is_suspended() && !this->write_uncertain_ &&
                       this->write_phase_ == WritePhase::IDLE;
  this->write_arm_at_ = App.get_loop_component_start_time();
}

bool ResolDeltaSolCSPlus::writes_armed() const {
  return this->write_armed_ && !this->stopped_ && !this->polling_->is_suspended() && !this->write_uncertain_ &&
         App.get_loop_component_start_time() - this->write_arm_at_ < 60000;
}

bool ResolDeltaSolCSPlus::request_setting(uint16_t index, float value) {
  if (this->write_phase_ != WritePhase::IDLE)
    return false;
  const auto spec = cs_plus::find_setting(index);
  if (this->stopped_ || this->polling_->is_suspended() || this->is_scanning() || this->write_uncertain_ ||
      !this->telemetry_online() || !this->has_fresh_snapshot() || (this->require_write_arm_ && !this->writes_armed()) ||
      !spec.has_value() || !std::isfinite(value)) {
    this->write_status_ = SettingWriteStatus::SETTING_WRITE_STATUS_REJECTED;
    return false;
  }
  const double scaled = static_cast<double>(value) / spec->scale;
  if (scaled < spec->minimum - 0.001 || scaled > spec->maximum + 0.001) {
    this->write_status_ = SettingWriteStatus::SETTING_WRITE_STATUS_REJECTED;
    return false;
  }
  const int32_t raw = static_cast<int32_t>(std::lround(scaled));
  const auto get = [this](uint16_t id) {
    int32_t current = 0;
    this->get_value(id, current);
    return current;
  };
  if (std::abs(scaled - raw) > 0.001 || !cs_plus::setting_allowed(*spec, raw, get)) {
    this->write_status_ = SettingWriteStatus::SETTING_WRITE_STATUS_REJECTED;
    return false;
  }
  this->write_armed_ = false;
  this->write_index_ = index;
  this->write_value_ = raw;
  this->writes_before_ = this->transmitter_->get_write_count();
  this->write_started_at_ = App.get_loop_component_start_time();
  this->write_verified_ = false;
  this->write_phase_ = WritePhase::PREFLIGHT;
  this->write_status_ = SettingWriteStatus::SETTING_WRITE_STATUS_PREFLIGHT;
  this->planned_.reset();
  this->received_.reset();
  this->working_values_.fill(0);
  this->plan_count_ = this->position_ = 0;
  for (size_t i = 0; i < spec->dependency_count; ++i)
    this->add_read_(spec->dependencies[i]);
  this->start_group_();
  return this->write_phase_ != WritePhase::IDLE;
}

void ResolDeltaSolCSPlus::advance_write_() {
  if (this->write_phase_ == WritePhase::VERIFY) {
    this->finish_write_(this->write_verified_ ? SettingWriteStatus::SETTING_WRITE_STATUS_SUCCESS
                                              : SettingWriteStatus::SETTING_WRITE_STATUS_UNCERTAIN);
    return;
  }
  const auto spec = cs_plus::find_setting(this->write_index_);
  if (!spec.has_value() ||
      !cs_plus::setting_allowed(*spec, this->write_value_, [this](uint16_t id) { return this->working_(id); })) {
    this->finish_write_(SettingWriteStatus::SETTING_WRITE_STATUS_FAILED);
    return;
  }
  if (this->working_(this->write_index_) == this->write_value_) {
    this->finish_write_(SettingWriteStatus::SETTING_WRITE_STATUS_UNCHANGED);
    return;
  }
  this->write_phase_ = WritePhase::VERIFY;
  this->write_status_ = SettingWriteStatus::SETTING_WRITE_STATUS_VERIFYING;
  this->phase_ = Phase::PHASE_GROUP;
  this->remaining_ = 1;
  this->group_at_ = App.get_loop_component_start_time();
  this->releases_before_group_ = this->transmitter_->get_release_count();
  this->resumed_ = false;
  this->polling_->resume_group();
  if (!this->polling_->queue_write(this->write_index_, static_cast<uint32_t>(this->write_value_)))
    this->finish_write_(SettingWriteStatus::SETTING_WRITE_STATUS_FAILED);
}

void ResolDeltaSolCSPlus::on_write_result_(uint16_t index, uint32_t value, TransactionStatus status) {
  if (this->phase_ != Phase::PHASE_GROUP || index != this->write_index_)
    return;
  if (status != TransactionStatus::TRANSACTION_STATUS_SUCCESS || static_cast<int32_t>(value) != this->write_value_) {
    this->finish_write_(this->transmitter_->get_write_count() != this->writes_before_
                            ? SettingWriteStatus::SETTING_WRITE_STATUS_UNCERTAIN
                            : SettingWriteStatus::SETTING_WRITE_STATUS_FAILED);
    return;
  }
  this->write_verified_ = true;
  this->remaining_ = 0;
}

void ResolDeltaSolCSPlus::finish_write_(SettingWriteStatus status) {
  if (this->write_phase_ == WritePhase::IDLE)
    return;
  const uint16_t index = this->write_index_;
  const auto spec = cs_plus::find_setting(index);
  this->write_phase_ = WritePhase::IDLE;
  this->phase_ = Phase::PHASE_IDLE;
  this->write_status_ = status;
  this->write_armed_ = false;
  this->disable_loop();
  if (status == SettingWriteStatus::SETTING_WRITE_STATUS_SUCCESS ||
      status == SettingWriteStatus::SETTING_WRITE_STATUS_UNCHANGED) {
    const auto position = cs_plus::setting_position(index);
    if (position.has_value()) {
      this->snapshot_[*position] = this->write_value_;
      this->snapshot_present_.set(*position);
    }
    if (status == SettingWriteStatus::SETTING_WRITE_STATUS_SUCCESS) {
      ++this->writes_verified_;
      if (spec.has_value() && spec->enum_mask != 0) {
        this->has_snapshot_ = false;
        this->context_refresh_pending_ = true;
      }
    }
    this->notify_snapshot_();
  } else {
    this->write_uncertain_ = status == SettingWriteStatus::SETTING_WRITE_STATUS_UNCERTAIN;
    if (this->write_uncertain_) {
      this->has_snapshot_ = false;
      this->context_refresh_pending_ = true;
    }
    // Reuse cancellation to discard queued writes and any pending verification.
    if (!this->polling_->is_suspended()) {
      this->polling_->suspend();
      this->polling_->resume();
    }
    ESP_LOGW(TAG, "Setting %u was not confirmed; no write retry", index);
  }
  if (this->context_refresh_pending_)
    this->enable_loop();
  this->setting_result_callback_.call(index, status);
}

}  // namespace esphome::vbus
#endif
