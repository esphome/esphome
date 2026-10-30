#include "resol_deltasol_cs_plus.h"

#ifdef USE_VBUS_CS_PLUS_WRITE
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cinttypes>
#include <cstdio>

namespace esphome::vbus {

ESPHOME_LOG_TAG(TAG, "vbus.cs_plus.write");

void ResolDeltaSolCSPlus::arm_writes(bool enabled) {
  this->write_armed_ = enabled && !this->stopped_ && !this->polling_->is_suspended() && !this->write_uncertain_ &&
                       !this->write_cleanup_ && this->write_phase_ == WritePhase::WRITE_PHASE_IDLE;
  this->write_arm_at_ = App.get_loop_component_start_time();
  if (enabled && !this->write_armed_) {
    ESP_LOGW(TAG, "Write arming rejected: stopped/suspended, uncertain write or active write");
  }
}

bool ResolDeltaSolCSPlus::writes_armed() const {
  return this->write_armed_ && !this->stopped_ && !this->polling_->is_suspended() && !this->write_uncertain_ &&
         !this->write_cleanup_ && App.get_loop_component_start_time() - this->write_arm_at_ < 60000;
}

bool ResolDeltaSolCSPlus::request_setting(uint16_t index, float value) {
  const auto reject = [this, index](const LogString *reason) {
    ESP_LOGW(TAG, "Setting %u request rejected: %s", index, LOG_STR_ARG(reason));
    if (this->write_phase_ == WritePhase::WRITE_PHASE_IDLE && !this->write_cleanup_)
      this->write_status_ = SettingWriteStatus::SETTING_WRITE_STATUS_REJECTED;
#ifdef USE_VBUS_WRITE_RESULT
    this->publish_write_result_(index, SettingWriteStatus::SETTING_WRITE_STATUS_REJECTED, reason);
#endif
    this->setting_result_callback_.call(index, SettingWriteStatus::SETTING_WRITE_STATUS_REJECTED);
    return false;
  };
  if (this->write_phase_ != WritePhase::WRITE_PHASE_IDLE || this->write_cleanup_)
    return reject(LOG_STR("another write is active"));
  const auto spec = cs_plus::find_setting(index);
  if (this->stopped_ || this->polling_->is_suspended())
    return reject(LOG_STR("controller stopped or suspended"));
  if (this->is_scanning())
    return reject(LOG_STR("settings scan in progress"));
  if (this->write_uncertain_)
    return reject(LOG_STR("previous write unconfirmed; refresh settings first"));
  if (!this->telemetry_online())
    return reject(LOG_STR("controller telemetry unavailable"));
  if (!this->has_fresh_snapshot())
    return reject(LOG_STR("settings context is stale; refresh settings first"));
  if (this->require_write_arm_ && !this->writes_armed())
    return reject(LOG_STR("writes are not armed"));
  if (!spec.has_value() || !std::isfinite(value))
    return reject(LOG_STR("unknown setting or non-finite value"));
  const double scaled = static_cast<double>(value) / spec->scale;
  if (scaled < spec->minimum - 0.001 || scaled > spec->maximum + 0.001) {
    return reject(LOG_STR("value outside permitted range"));
  }
  const int32_t raw = static_cast<int32_t>(std::lround(scaled));
  const auto get = [this](uint16_t id) {
    int32_t current = 0;
    this->get_value(id, current);
    return current;
  };
  if (std::abs(scaled - raw) > 0.001 || !cs_plus::setting_allowed(*spec, raw, get)) {
    return reject(LOG_STR("value not allowed by the current settings"));
  }
  this->write_armed_ = false;
  this->write_index_ = index;
  this->write_value_ = raw;
  this->writes_before_ = this->transmitter_->get_write_count();
  this->write_started_at_ = App.get_loop_component_start_time();
  this->write_verified_ = false;
  this->write_phase_ = WritePhase::WRITE_PHASE_PREFLIGHT;
  this->write_status_ = SettingWriteStatus::SETTING_WRITE_STATUS_PREFLIGHT;
  this->planned_.reset();
  this->received_.reset();
  this->working_values_.fill(0);
  this->plan_count_ = this->position_ = 0;
  for (size_t i = 0; i < spec->dependency_count; ++i)
    this->add_read_(spec->dependencies[i]);
  this->start_group_();
  return this->write_phase_ != WritePhase::WRITE_PHASE_IDLE;
}

void ResolDeltaSolCSPlus::advance_write_() {
  if (this->write_phase_ == WritePhase::WRITE_PHASE_VERIFY) {
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
  this->write_phase_ = WritePhase::WRITE_PHASE_VERIFY;
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
  if (this->write_phase_ == WritePhase::WRITE_PHASE_IDLE)
    return;
  const uint16_t index = this->write_index_;
#ifdef USE_VBUS_WRITE_RESULT
  const int32_t value = this->write_value_;
#endif
  const auto spec = cs_plus::find_setting(index);
  this->write_phase_ = WritePhase::WRITE_PHASE_IDLE;
  this->phase_ = Phase::PHASE_IDLE;
  this->write_status_ = status;
  this->write_armed_ = false;
  this->disable_loop();
  if (status == SettingWriteStatus::SETTING_WRITE_STATUS_SUCCESS ||
      status == SettingWriteStatus::SETTING_WRITE_STATUS_UNCHANGED) {
#ifdef USE_VBUS_CS_PLUS_ADAPTIVE
    if (this->context_update_interval_ != 0)
      this->finish_adaptive_write_();
#endif
    const auto position = cs_plus::setting_position(index);
    if (position.has_value()) {
      this->snapshot_[*position] = this->write_value_;
      this->snapshot_present_.set(*position);
    }
    if (status == SettingWriteStatus::SETTING_WRITE_STATUS_SUCCESS) {
      ++this->writes_verified_;
      if (spec.has_value() && spec->enum_mask != 0
#ifdef USE_VBUS_CS_PLUS_ADAPTIVE
          && this->context_update_interval_ == 0
#endif
      ) {
        this->has_snapshot_ = false;
        this->context_refresh_pending_ = true;
      }
    }
    this->notify_snapshot_();
  } else {
    this->write_uncertain_ = status == SettingWriteStatus::SETTING_WRITE_STATUS_UNCERTAIN;
    if (this->write_uncertain_) {
#ifdef USE_VBUS_CS_PLUS_ADAPTIVE
      this->partial_refresh_pending_ = false;
#endif
      this->has_snapshot_ = false;
      this->context_refresh_pending_ = true;
    }
    if (!this->polling_->is_suspended()) {
      this->write_cleanup_ = true;
      this->polling_->cancel_pending();
      this->write_cleanup_ = false;
      // Reconcile the context without reporting an OTA suspension/resume.
      this->resume_pending_ = !this->stopped_;
    }
    ESP_LOGW(TAG, "Setting %u was not confirmed; no write retry", index);
  }
  if (this->context_refresh_pending_)
    this->enable_loop();
#ifdef USE_VBUS_WRITE_RESULT
  this->publish_write_result_(index, status, nullptr, value);
#endif
  this->setting_result_callback_.call(index, status);
}

#ifdef USE_VBUS_WRITE_RESULT
void ResolDeltaSolCSPlus::publish_write_result_(uint16_t index, SettingWriteStatus status, const LogString *reason,
                                                int32_t raw_value) {
  if (this->last_write_result_text_sensor_ == nullptr)
    return;
  if (reason == nullptr) {
    switch (status) {
      case SettingWriteStatus::SETTING_WRITE_STATUS_SUCCESS:
        reason = LOG_STR("verified");
        break;
      case SettingWriteStatus::SETTING_WRITE_STATUS_UNCHANGED:
        reason = LOG_STR("already set; no write needed");
        break;
      case SettingWriteStatus::SETTING_WRITE_STATUS_UNCERTAIN:
        reason = LOG_STR("unconfirmed; settings refresh required");
        break;
      default:
        reason = LOG_STR("failed; no automatic retry");
        break;
    }
  }
  char detail[112];
  ESPHOME_strncpy_P(detail, LOG_STR_ARG(reason), sizeof(detail) - 1);
  detail[sizeof(detail) - 1] = '\0';
  char result[144];
  if (status == SettingWriteStatus::SETTING_WRITE_STATUS_SUCCESS ||
      status == SettingWriteStatus::SETTING_WRITE_STATUS_UNCHANGED) {
    const auto spec = cs_plus::find_setting(index);
    const float scale = spec.has_value() ? spec->scale : 1.0f;
    char scaled[VALUE_ACCURACY_MAX_LEN];
    value_accuracy_to_buf(scaled, raw_value * scale, step_to_accuracy_decimals(scale));
    snprintf(result, sizeof(result), "Setting %u: %s (value %s, raw %" PRId32 ")", index, detail, scaled, raw_value);
  } else {
    snprintf(result, sizeof(result), "Setting %u: %s", index, detail);
  }
  if (this->last_write_result_text_sensor_->get_raw_state() != result)
    this->last_write_result_text_sensor_->publish_state(result);
}
#endif

}  // namespace esphome::vbus
#endif
