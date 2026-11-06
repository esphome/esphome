#include "resol_deltasol_cs_plus.h"

#ifdef USE_VBUS_CS_PLUS_ADAPTIVE
#include "esphome/core/application.h"
#include <cmath>

namespace esphome::vbus {

float ResolDeltaSolCSPlus::value_age_seconds(uint16_t index) const {
  if (index == 2)
    return this->clock_valid_ ? (App.get_loop_component_start_time() - this->telemetry_at_) / 1000.0f : NAN;
  const auto position = cs_plus::setting_position(index);
  if (!this->snapshot_retained_ || !position.has_value() || !this->snapshot_present_[*position])
    return NAN;
  return (App.get_loop_component_start_time() - this->sampled_at_[*position]) / 1000.0f;
}

void ResolDeltaSolCSPlus::update_adaptive_() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->resume_pending_ || !this->attempted_) {
    if (now - this->attempt_at_ >= (this->attempted_ ? 60000 : 30000))
      this->start_scan_(false);
  } else if (now - this->full_attempt_at_ >= this->get_update_interval()) {
    this->start_scan_(false);
  } else if (now - this->context_attempt_at_ >= this->context_update_interval_) {
    this->start_scan_(true);
  }
}

bool ResolDeltaSolCSPlus::needs_refresh_(const cs_plus::SettingSpec &spec) const {
  const int32_t scheme = this->working_(1);
  for (size_t j = 0; j < spec.dependency_count; ++j) {
    const auto dependency = cs_plus::find_setting(spec.dependencies[j]);
    if (dependency.has_value() && dependency->enum_mask != 0 && !(dependency->schemes & (1u << scheme)))
      continue;
    const auto position = cs_plus::setting_position(spec.dependencies[j]);
    if (!position.has_value())
      continue;
    if (!this->snapshot_present_[*position] || this->refresh_changed_[*position] ||
        (this->received_[*position] && this->working_values_[*position] != this->snapshot_[*position]) ||
        App.get_loop_component_start_time() - this->sampled_at_[*position] > this->snapshot_max_age_())
      return true;
  }
  return false;
}

void ResolDeltaSolCSPlus::commit_adaptive_() {
  const uint32_t now = App.get_loop_component_start_time();
  if (!this->partial_scan_) {
    this->snapshot_present_ = this->received_;
    this->snapshot_at_ = now;
  }
  for (size_t i = 0; i < COUNT; ++i) {
    if (!this->received_[i])
      continue;
    this->snapshot_[i] = this->working_values_[i];
    this->snapshot_present_.set(i);
    this->sampled_at_[i] = now;
  }
  this->context_at_ = now;
  this->refresh_changed_.reset();
}

#ifdef USE_VBUS_CS_PLUS_WRITE
void ResolDeltaSolCSPlus::finish_adaptive_write_() {
  const uint32_t now = App.get_loop_component_start_time();
  const auto target = cs_plus::setting_position(this->write_index_);
  if (target.has_value()) {
    this->working_values_[*target] = this->write_value_;
    this->received_.set(*target);
  }
  this->refresh_changed_.reset();
  for (size_t i = 0; i < COUNT; ++i) {
    if (!this->received_[i])
      continue;
    if (!this->snapshot_present_[i] || this->snapshot_[i] != this->working_values_[i])
      this->refresh_changed_.set(i);
    this->snapshot_[i] = this->working_values_[i];
    this->snapshot_present_.set(i);
    this->sampled_at_[i] = now;
  }
  bool affected = false;
  for (size_t i = 0; i < COUNT && !affected; ++i) {
    const auto spec = cs_plus::setting_at(i);
    if (!spec.has_value())
      continue;
    for (size_t j = 0; j < spec->dependency_count; ++j) {
      const auto position = cs_plus::setting_position(spec->dependencies[j]);
      if (spec->dependencies[j] != spec->index && position.has_value() && this->refresh_changed_[*position]) {
        affected = true;
        break;
      }
    }
  }
  if (affected) {
    this->has_snapshot_ = false;
    this->context_refresh_pending_ = true;
    const auto scheme_position = cs_plus::setting_position(1);
    const auto unit_position = cs_plus::setting_position(10);
    this->partial_refresh_pending_ = scheme_position.has_value() && unit_position.has_value() &&
                                     !this->refresh_changed_[*scheme_position] &&
                                     !this->refresh_changed_[*unit_position];
  }
}
#endif

}  // namespace esphome::vbus
#endif
