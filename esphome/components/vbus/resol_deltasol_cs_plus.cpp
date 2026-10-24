#include "resol_deltasol_cs_plus.h"

#ifdef USE_VBUS_CS_PLUS
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome::vbus {

ESPHOME_LOG_TAG(TAG, "vbus.cs_plus");
static constexpr uint32_t SNAPSHOT_MAX_AGE_MS = 660000;
static constexpr uint32_t GROUP_TIMEOUT_MS = 15000;
static constexpr uint32_t MIN_SCAN_INTERVAL_MS = 60000;

ResolDeltaSolCSPlus::ResolDeltaSolCSPlus(VBus *receiver, VBusPolling *polling, VBusTransmitter *transmitter)
    : receiver_(receiver), polling_(polling), transmitter_(transmitter) {
  this->set_source(0x2211);
  this->set_dest(0x0010);
  this->set_command(0x0100);
  for (size_t i = 0; i < COUNT; ++i) {
    const auto spec = cs_plus::setting_at(i);
    if (spec.has_value())
      this->polling_->register_value(spec->index, false);
  }
}

void ResolDeltaSolCSPlus::setup() {
  this->stop_poller();
  this->attempt_at_ = App.get_loop_component_start_time();
  this->receiver_->register_listener(this);
  this->transmitter_->add_on_bus_grant_callback([this](uint32_t) {
    if (this->phase_ == Phase::PHASE_GROUP)
      ++this->group_count_;
  });
  this->polling_->add_on_result_callback(
      [this](uint32_t grant, uint16_t command, uint16_t index, uint32_t value, TransactionStatus status) {
        this->on_result_(grant, command, index, value, status);
      });
  this->polling_->add_on_group_end_callback([this] {
    if (this->phase_ == Phase::PHASE_GROUP) {
      this->phase_ = Phase::PHASE_TELEMETRY;
      this->resumed_ = false;
    }
  });
  this->polling_->add_on_suspended_callback([this](bool suspended) {
    this->resume_pending_ =
        !suspended && !this->stopped_ && (this->attempted_ || this->get_update_interval() != SCHEDULER_DONT_RUN);
    if (suspended) {
      this->abort_();
    }
  });
  this->set_interval("age", 1000, [this] {
    this->check_age_();
    this->update();
  });
  this->disable_loop();
}

void ResolDeltaSolCSPlus::dump_config() {
  ESP_LOGCONFIG(TAG, "RESOL DeltaSol CS Plus:\n  Firmware: 1.11\n  Snapshot expiry: 660s\n  Read batch: 16");
  LOG_UPDATE_INTERVAL(this);
}

bool ResolDeltaSolCSPlus::telemetry_online() const {
  return this->profile_ok_ && this->telemetry_count_ != 0 &&
         App.get_loop_component_start_time() - this->telemetry_at_ < 15000;
}

bool ResolDeltaSolCSPlus::has_fresh_snapshot() const {
  return this->has_snapshot_ && this->profile_ok_ &&
         App.get_loop_component_start_time() - this->snapshot_at_ <= SNAPSHOT_MAX_AGE_MS;
}

float ResolDeltaSolCSPlus::snapshot_age_seconds() const {
  return this->has_snapshot_ ? (App.get_loop_component_start_time() - this->snapshot_at_) / 1000.0f : NAN;
}

bool ResolDeltaSolCSPlus::get_value(uint16_t index, int32_t &value) const {
  if (!this->has_fresh_snapshot())
    return false;
  if (index == 2) {
    value = this->clock_minutes_;
    return true;
  }
  const auto position = cs_plus::setting_position(index);
  if (!position.has_value() || !this->snapshot_present_[*position])
    return false;
  value = this->snapshot_[*position];
  return true;
}

void ResolDeltaSolCSPlus::check_age_() {
  if (this->stopped_)
    return;
  if (this->has_snapshot_ && App.get_loop_component_start_time() - this->snapshot_at_ > SNAPSHOT_MAX_AGE_MS) {
    this->has_snapshot_ = false;
    this->snapshot_callback_.call();
  }
}

void ResolDeltaSolCSPlus::handle_message(std::vector<uint8_t> &message) {
  if (this->stopped_ || message.size() < 40)
    return;
  this->profile_ok_ = message[32] == 111 && message[33] == 0;
  if (!this->profile_ok_) {
    this->abort_();
    if (this->has_snapshot_) {
      this->has_snapshot_ = false;
      this->snapshot_callback_.call();
    }
    return;
  }
  this->clock_minutes_ = message[22] | (uint16_t(message[23]) << 8);
  ++this->telemetry_count_;
  this->telemetry_at_ = App.get_loop_component_start_time();
  if (this->phase_ == Phase::PHASE_TELEMETRY &&
      this->transmitter_->get_release_count() != this->releases_before_group_) {
    this->resumed_ = true;
  }
}

void ResolDeltaSolCSPlus::update() {
  const uint32_t elapsed = App.get_loop_component_start_time() - this->attempt_at_;
  if (this->resume_pending_) {
    if (elapsed >= (this->attempted_ ? MIN_SCAN_INTERVAL_MS : 30000))
      this->start_scan();
  } else if (this->get_update_interval() != SCHEDULER_DONT_RUN &&
             elapsed >= (this->attempted_ ? this->get_update_interval() : 30000)) {
    this->start_scan();
  }
}

bool ResolDeltaSolCSPlus::start_scan() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->stopped_ || this->phase_ != Phase::PHASE_IDLE || this->polling_->is_suspended() ||
      !this->telemetry_online() || (this->attempted_ && now - this->attempt_at_ < MIN_SCAN_INTERVAL_MS))
    return false;
  this->attempted_ = true;
  this->resume_pending_ = false;
  this->attempt_at_ = now;
  this->planned_.reset();
  this->received_.reset();
  this->working_values_.fill(0);
  this->plan_count_ = this->position_ = 0;
  this->request_count_ = this->received_count_ = this->group_count_ = this->release_count_ = 0;
  this->elapsed_ms_ = 0;
  this->maximum_lease_ms_ = 0;
  this->reads_at_start_ = this->transmitter_->get_read_count();
  this->releases_at_start_ = this->transmitter_->get_release_count();
  this->plan_stage_ = 0;
  this->add_read_(1);
  this->add_read_(10);
  this->start_group_();
  return this->phase_ != Phase::PHASE_IDLE;
}

void ResolDeltaSolCSPlus::add_read_(uint16_t index) {
  const auto position = cs_plus::setting_position(index);
  if (position.has_value() && !this->planned_[*position]) {
    this->planned_.set(*position);
    this->plan_[this->plan_count_++] = index;
  }
}

int32_t ResolDeltaSolCSPlus::working_(uint16_t index) const {
  const auto position = cs_plus::setting_position(index);
  if (!position.has_value() || !this->received_[*position])
    return 0;
  return this->working_values_[*position];
}

void ResolDeltaSolCSPlus::extend_plan_() {
  const int32_t scheme = this->working_(1);
  const auto get = [this, scheme](uint16_t index) {
    const auto spec = cs_plus::find_setting(index);
    return spec.has_value() && spec->enum_mask != 0 && !(spec->schemes & (1u << scheme)) ? 0 : this->working_(index);
  };
  if (this->plan_stage_ == 0) {
    this->plan_stage_ = 1;
    static constexpr uint16_t FLAGS[] = {21,   2768, 12549, 12550, 81,  74,  85,  90,   71,  64,
                                         2569, 461,  462,   512,   513, 544, 545, 2560, 3897};
    for (uint16_t index : FLAGS) {
      const auto spec = cs_plus::find_setting(index);
      if (spec.has_value() && (spec->schemes & (1u << scheme)))
        this->add_read_(index);
    }
    if (this->position_ < this->plan_count_)
      return;
  }
  if (this->plan_stage_ == 1) {
    this->plan_stage_ = 2;
    for (size_t i = 0; i < COUNT; ++i) {
      const auto spec = cs_plus::setting_at(i);
      if (!spec.has_value() || spec->index == 2 || !cs_plus::setting_visible(*spec, get))
        continue;
      for (size_t j = 0; j < spec->dependency_count; ++j) {
        const auto dependency = cs_plus::find_setting(spec->dependencies[j]);
        if (dependency.has_value() && (dependency->enum_mask == 0 || (dependency->schemes & (1u << scheme))))
          this->add_read_(dependency->index);
      }
    }
  }
}

void ResolDeltaSolCSPlus::start_group_() {
  this->polling_->resume_group();
  const size_t end = std::min(this->position_ + 16, this->plan_count_);
  this->remaining_ = end - this->position_;
  this->releases_before_group_ = this->transmitter_->get_release_count();
  this->resumed_ = false;
  this->group_at_ = App.get_loop_component_start_time();
  this->phase_ = Phase::PHASE_GROUP;
  for (; this->position_ < end; ++this->position_) {
    if (!this->polling_->queue_read(this->plan_[this->position_])) {
      this->abort_();
      return;
    }
  }
  this->enable_loop();
}

void ResolDeltaSolCSPlus::on_result_(uint32_t, uint16_t command, uint16_t index, uint32_t value,
                                     TransactionStatus status) {
  if (this->phase_ != Phase::PHASE_GROUP || command != 0x0300)
    return;
  const auto position = cs_plus::setting_position(index);
  const auto spec = cs_plus::find_setting(index);
  if (status != TransactionStatus::TRANSACTION_STATUS_SUCCESS || !position.has_value() || !spec.has_value() ||
      !this->planned_[*position] || this->received_[*position] ||
      !cs_plus::setting_shape_valid(*spec, static_cast<int32_t>(value))) {
    this->abort_();
    return;
  }
  this->working_values_[*position] = static_cast<int32_t>(value);
  this->received_.set(*position);
  ++this->received_count_;
  --this->remaining_;
}

void ResolDeltaSolCSPlus::loop() {
  if (this->stopped_ || this->phase_ == Phase::PHASE_IDLE)
    return;
  this->request_count_ = this->transmitter_->get_read_count() - this->reads_at_start_;
  const uint32_t releases = this->transmitter_->get_release_count() - this->releases_at_start_;
  if (releases != this->release_count_)
    this->maximum_lease_ms_ = std::max(this->maximum_lease_ms_, this->transmitter_->get_last_release_duration());
  this->release_count_ = releases;
  if (this->polling_->is_suspended() || !this->telemetry_online() ||
      App.get_loop_component_start_time() - this->group_at_ >= GROUP_TIMEOUT_MS) {
    this->abort_();
    return;
  }
  if (this->phase_ != Phase::PHASE_TELEMETRY || !this->resumed_)
    return;
  if (this->remaining_ != 0) {
    this->phase_ = Phase::PHASE_GROUP;
    this->resumed_ = false;
    this->releases_before_group_ = this->transmitter_->get_release_count();
    this->group_at_ = App.get_loop_component_start_time();
    this->polling_->resume_group();
    return;
  }
  if (this->position_ == this->plan_count_)
    this->extend_plan_();
  if (this->position_ < this->plan_count_) {
    this->start_group_();
    return;
  }
  this->snapshot_ = this->working_values_;
  this->snapshot_present_ = this->received_;
  this->snapshot_at_ = App.get_loop_component_start_time();
  this->elapsed_ms_ = this->snapshot_at_ - this->attempt_at_;
  ++this->completed_scans_;
  this->has_snapshot_ = true;
  this->phase_ = Phase::PHASE_IDLE;
  this->disable_loop();
  this->snapshot_callback_.call();
}

void ResolDeltaSolCSPlus::abort_() {
  if (this->phase_ == Phase::PHASE_IDLE)
    return;
  ESP_LOGW(TAG, "Scan incomplete; retaining the previous snapshot");
  this->request_count_ = this->transmitter_->get_read_count() - this->reads_at_start_;
  this->release_count_ = this->transmitter_->get_release_count() - this->releases_at_start_;
  this->phase_ = Phase::PHASE_IDLE;
  this->polling_->clear_pending_reads();
  this->elapsed_ms_ = App.get_loop_component_start_time() - this->attempt_at_;
  this->disable_loop();
}

void ResolDeltaSolCSPlus::on_shutdown() {
  this->stopped_ = true;
  this->abort_();
  this->polling_->on_shutdown();
  this->stop_poller();
  this->cancel_interval("age");
}

}  // namespace esphome::vbus
#endif
