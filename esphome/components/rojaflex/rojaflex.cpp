#include "rojaflex.h"

#include "esphome/core/log.h"

#include "cover/rojaflex_cover.h"

namespace esphome::rojaflex {

static const char *const TAG = "rojaflex";

void RojaflexComponent::setup() {
  if (this->transceiver_ == nullptr) {
    ESP_LOGE(TAG, "No cc1101 transceiver configured");
    this->mark_failed();
    return;
  }
  this->transceiver_->register_listener(this);
  this->set_interval("position_interpolation", 500, [this]() { this->tick_position_interpolation_(); });
}

void RojaflexComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Rojaflex:");
  ESP_LOGCONFIG(TAG, "  Housecode: %s", this->housecode_.c_str());
  ESP_LOGCONFIG(TAG, "  TX repetitions: %u", static_cast<unsigned>(this->tx_repetitions_));
}

void RojaflexComponent::register_device(RojaflexDevice *device) { this->devices_.push_back(device); }

void RojaflexComponent::register_cover(RojaflexCover *cover, uint8_t channel) {
  if (channel < this->covers_.size()) {
    this->covers_[channel] = cover;
  }
}

bool RojaflexComponent::set_housecode_manually(const std::string &housecode) {
  if (!apply_manual_housecode(housecode, this->housecode_, this->auto_learn_housecode_, this->auto_learn_count_)) {
    ESP_LOGE(TAG, "Invalid housecode format. Must be 7 hex characters.");
    return false;
  }
  ESP_LOGI(TAG, "Shared housecode set to: %s", this->housecode_.c_str());
  return true;
}

int RojaflexComponent::get_motor_pct(uint8_t channel_id) const {
  if (channel_id >= this->motor_pct_.size()) {
    return -1;
  }
  return this->motor_pct_[channel_id];
}

int RojaflexComponent::get_cal_time_open_s(uint8_t channel_id) const {
  if (channel_id >= this->cal_time_open_s_.size()) {
    return -1;
  }
  return this->cal_time_open_s_[channel_id];
}

int RojaflexComponent::get_cal_time_close_s(uint8_t channel_id) const {
  if (channel_id >= this->cal_time_close_s_.size()) {
    return -1;
  }
  return this->cal_time_close_s_[channel_id];
}

std::string RojaflexComponent::get_channel_status(uint8_t channel_id) const {
  if (channel_id >= this->motor_pct_.size()) {
    return "config error";
  }
  if (channel_id == 0) {
    return "Broadcast channel - end stops only";
  }

  const int pct = this->motor_pct_[channel_id];
  const int t_open = this->get_cal_time_open_s(channel_id);
  const int t_close = this->get_cal_time_close_s(channel_id);
  if (pct < 0) {
    return "Calibration needed: drive fully open or closed once";
  }
  const bool open_ok = t_open >= 0;
  const bool close_ok = t_close >= 0;
  if (!open_ok && !close_ok) {
    return "Calibration needed: drive both end stops once";
  }
  if (!open_ok) {
    return "Calibration needed: drive fully open once";
  }
  if (!close_ok) {
    return "Calibration needed: drive fully closed once";
  }
  char buf[64];
  snprintf(buf, sizeof(buf), "Calibrated (open: %ds, close: %ds)", t_open, t_close);
  return std::string(buf);
}

std::string RojaflexComponent::get_device_status() const {
  if (!is_housecode_configured(this->housecode_)) {
    if (!this->auto_learn_housecode_.empty()) {
      char status_buf[64];
      snprintf(status_buf, sizeof(status_buf), "AUTO-LEARNING: %s (%u/%u)", this->auto_learn_housecode_.c_str(),
               static_cast<unsigned>(this->auto_learn_count_), static_cast<unsigned>(AUTO_LEARN_FRAMES_REQUIRED));
      return std::string(status_buf);
    }
    return "Waiting for housecode...";
  }
  return "Ready";
}

std::string RojaflexComponent::mid_position_timer_name_(uint8_t channel_id) const {
  char buf[32];
  snprintf(buf, sizeof(buf), "rojaflex_pct_stop_%u", static_cast<unsigned>(channel_id));
  return std::string(buf);
}

void RojaflexComponent::cancel_mid_position_timers_(uint8_t channel_id, bool all_channels) {
  if (all_channels) {
    for (uint32_t ch = 0; ch < 16; ch++) {
      const auto timer_name = this->mid_position_timer_name_(ch);
      App.scheduler.cancel_timeout(this, timer_name.c_str());
    }
    return;
  }
  const auto cancel_timer_name = this->mid_position_timer_name_(channel_id);
  App.scheduler.cancel_timeout(this, cancel_timer_name.c_str());
}

void RojaflexComponent::apply_pct_to_motor_(uint8_t channel_id, int pct, bool all_channels) {
  if (all_channels) {
    for (auto &value : this->motor_pct_) {
      value = pct;
    }
  } else if (channel_id < this->motor_pct_.size()) {
    this->motor_pct_[channel_id] = pct;
  }
  this->refresh_covers_();
}

void RojaflexComponent::cancel_position_interpolation_(uint8_t channel_id) {
  if (channel_id < this->interp_start_ms_.size()) {
    this->interp_start_ms_[channel_id] = 0;
  }
}

void RojaflexComponent::arm_position_interpolation_(uint8_t channel_id, int start_pct, int end_pct, uint32_t duration_ms) {
  if (channel_id == 0 || duration_ms == 0 || start_pct == end_pct || start_pct < 0 || end_pct < 0) {
    this->cancel_position_interpolation_(channel_id);
    return;
  }
  this->interp_start_ms_[channel_id] = millis();
  this->interp_start_pct_[channel_id] = start_pct;
  this->interp_end_pct_[channel_id] = end_pct;
  this->interp_duration_ms_[channel_id] = duration_ms;
}

void RojaflexComponent::tick_position_interpolation_() {
  const uint32_t now = millis();
  for (uint8_t ch = 0; ch < this->interp_start_ms_.size(); ch++) {
    const uint32_t start_ms = this->interp_start_ms_[ch];
    if (start_ms == 0 || ch >= this->motor_pct_.size()) {
      continue;
    }
    const uint32_t duration = this->interp_duration_ms_[ch];
    if (duration == 0) {
      this->cancel_position_interpolation_(ch);
      continue;
    }
    const uint32_t elapsed = now - start_ms;
    if (elapsed >= duration) {
      continue;
    }
    const int start_pct = this->interp_start_pct_[ch];
    const int end_pct = this->interp_end_pct_[ch];
    const int delta = end_pct - start_pct;
    const int step = static_cast<int>((static_cast<int64_t>(delta) * static_cast<int64_t>(elapsed)) / static_cast<int64_t>(duration));
    this->motor_pct_[ch] = start_pct + step;
  }
  this->refresh_covers_();
}

void RojaflexComponent::cancel_calibration_capture_(uint8_t channel_id) {
  if (channel_id < this->cal_pending_tx_ms_.size()) {
    this->cal_pending_tx_ms_[channel_id] = 0;
    this->cal_pending_target_pct_[channel_id] = -1;
  }
}

void RojaflexComponent::arm_calibration_capture_(uint8_t channel_id, int target_pct) {
  if (channel_id == 0) {
    return;
  }
  this->cal_pending_tx_ms_[channel_id] = millis();
  this->cal_pending_target_pct_[channel_id] = target_pct;
}

bool RojaflexComponent::try_resolve_calibration_(uint8_t channel_id, int reported_pct) {
  if (channel_id >= this->cal_pending_tx_ms_.size()) {
    return false;
  }
  const uint32_t tx_ms = this->cal_pending_tx_ms_[channel_id];
  if (tx_ms == 0) {
    return false;
  }
  const int target = this->cal_pending_target_pct_[channel_id];
  if (target < 0) {
    return false;
  }
  const uint32_t elapsed = millis() - tx_ms;
  if (elapsed >= CALIBRATION_CAPTURE_WINDOW_MS) {
    this->cancel_calibration_capture_(channel_id);
    return false;
  }
  if (reported_pct != target) {
    return false;
  }
  const int delta_s = static_cast<int>(elapsed / 1000u);
  if (delta_s < CALIBRATION_MIN_TIME_S) {
    ESP_LOGW(TAG, "Calibration capture rejected for channel=%u: %ds < %d s sanity floor", static_cast<unsigned>(channel_id), delta_s,
             CALIBRATION_MIN_TIME_S);
    this->cancel_calibration_capture_(channel_id);
    return false;
  }
  if (target == 0) {
    this->cal_time_open_s_[channel_id] = delta_s;
    ESP_LOGI(TAG, "Calibrated channel=%u open time = %d s", static_cast<unsigned>(channel_id), delta_s);
  } else {
    this->cal_time_close_s_[channel_id] = delta_s;
    ESP_LOGI(TAG, "Calibrated channel=%u close time = %d s", static_cast<unsigned>(channel_id), delta_s);
  }
  this->cancel_calibration_capture_(channel_id);
  return true;
}

void RojaflexComponent::refresh_covers_() {
  for (uint8_t channel = 0; channel < this->covers_.size(); channel++) {
    if (this->covers_[channel] != nullptr) {
      this->covers_[channel]->sync_from_parent();
    }
  }
}

void RojaflexComponent::on_packet(const std::vector<uint8_t> &payload, float, float, uint8_t) {
  if (!is_valid_p109_payload(payload)) {
    return;
  }

  if (!is_housecode_configured(this->housecode_)) {
    auto learn = auto_learn_housecode_step(payload, this->housecode_, this->auto_learn_housecode_, this->auto_learn_count_);
    this->housecode_ = learn.configured_housecode;
    this->auto_learn_housecode_ = learn.candidate_housecode;
    this->auto_learn_count_ = learn.candidate_count;
    if (learn.learned_now) {
      ESP_LOGI(TAG, "AUTO-LEARNED housecode: %s", this->housecode_.c_str());
    }
    return;
  }

  const auto frame = decode_p109_frame(payload, this->housecode_);
  if (!frame.housecode_match) {
    return;
  }
  this->last_rx_raw_ = frame.raw;
  this->last_rx_info_ = frame.info;

  if (frame.position_source == PositionSource::None) {
    return;
  }

  if (frame.position_source == PositionSource::RemoteInferred && this->last_self_tx_ms_ != 0 &&
      (millis() - this->last_self_tx_ms_) < SELF_TX_ECHO_GUARD_MS) {
    return;
  }

  this->apply_pct_to_motor_(frame.channel, frame.pct, frame.applies_to_all_channels);

  if (frame.applies_to_all_channels) {
    for (uint8_t ch = 0; ch < 16; ch++) {
      this->cancel_position_interpolation_(ch);
    }
  } else {
    this->cancel_position_interpolation_(frame.channel);
  }

  if (frame.position_source == PositionSource::RemoteInferred) {
    this->cancel_mid_position_timers_(frame.channel, frame.applies_to_all_channels);
    if (frame.applies_to_all_channels) {
      for (uint8_t ch = 0; ch < 16; ch++) {
        this->cancel_calibration_capture_(ch);
      }
    } else {
      this->cancel_calibration_capture_(frame.channel);
    }
  }

  if (frame.position_source == PositionSource::MotorFeedback) {
    this->try_resolve_calibration_(frame.channel, frame.pct);
  }
}

bool RojaflexComponent::send_command(uint8_t channel_id, uint8_t cmd_code, bool optimistic_motor_pct_update) {
  if (this->transceiver_ == nullptr || channel_id > 15) {
    return false;
  }
  if (!is_housecode_configured(this->housecode_)) {
    ESP_LOGW(TAG, "Shared housecode not configured");
    return false;
  }

  std::vector<uint8_t> tx_packet;
  std::string final_msg;
  if (!build_tx_packet(this->housecode_, channel_id, cmd_code, tx_packet, final_msg)) {
    ESP_LOGE(TAG, "Failed to build TX packet");
    return false;
  }
  ESP_LOGI(TAG, "Sending P109 packet: %s (x%u)", final_msg.c_str(), static_cast<unsigned>(this->tx_repetitions_));

  bool any_tx_ok = false;
  int last_err = 0;
  for (uint8_t r = 0; r < this->tx_repetitions_; r++) {
    if (r > 0) {
      delay(80);
    }
    const auto err = this->transceiver_->transmit_packet(tx_packet);
    this->last_self_tx_ms_ = millis();
    const bool ok = (err == cc1101::CC1101Error::NONE);
    if (ok) {
      any_tx_ok = true;
    } else {
      last_err = static_cast<int>(err);
      ESP_LOGE(TAG, "TX repeat %u/%u failed (error code %d)", static_cast<unsigned>(r + 1), static_cast<unsigned>(this->tx_repetitions_),
               last_err);
    }
  }

  this->cc1101_last_tx_ok_ = any_tx_ok;
  this->cc1101_last_error_ = any_tx_ok ? 0 : last_err;
  if (!any_tx_ok) {
    return false;
  }

  this->cancel_position_interpolation_(channel_id);

  const int prev_pct = this->motor_pct_[channel_id];
  if (optimistic_motor_pct_update && cmd_code == static_cast<uint8_t>(Command::UP) && prev_pct == 100) {
    this->arm_calibration_capture_(channel_id, 0);
  } else if (optimistic_motor_pct_update && cmd_code == static_cast<uint8_t>(Command::DOWN) && prev_pct == 0) {
    this->arm_calibration_capture_(channel_id, 100);
  } else {
    this->cancel_calibration_capture_(channel_id);
  }

  if (!optimistic_motor_pct_update) {
    return true;
  }

  int new_pct = -1;
  int direction_time_s = -1;
  if (cmd_code == static_cast<uint8_t>(Command::UP)) {
    new_pct = 0;
    direction_time_s = this->get_cal_time_open_s(channel_id);
  } else if (cmd_code == static_cast<uint8_t>(Command::DOWN)) {
    new_pct = 100;
    direction_time_s = this->get_cal_time_close_s(channel_id);
  }

  if (new_pct >= 0) {
    const bool can_interpolate = (direction_time_s > 0) && (prev_pct >= 0) && (prev_pct != new_pct);
    if (can_interpolate) {
      const int delta_pct = (new_pct > prev_pct) ? (new_pct - prev_pct) : (prev_pct - new_pct);
      const uint32_t duration_ms =
          (static_cast<uint32_t>(delta_pct) * static_cast<uint32_t>(direction_time_s) * 1000u) / 100u;
      this->arm_position_interpolation_(channel_id, prev_pct, new_pct, duration_ms);
    } else {
      this->motor_pct_[channel_id] = new_pct;
      this->refresh_covers_();
    }
  }

  return true;
}

void RojaflexComponent::set_position(uint8_t channel_id, int target_pct) {
  if (channel_id > 15) {
    return;
  }
  if (channel_id == 0 && target_pct != 0 && target_pct != 100) {
    ESP_LOGW(TAG, "set_position channel=0 target=%d rejected: broadcast channel supports only end stops (0 or 100)", target_pct);
    return;
  }

  const int current_pct = this->motor_pct_[channel_id];
  const auto plan = compute_shutter_motion_plan(current_pct, target_pct, this->get_cal_time_open_s(channel_id),
                                                this->get_cal_time_close_s(channel_id));
  const auto timer_name = this->mid_position_timer_name_(channel_id);
  App.scheduler.cancel_timeout(this, timer_name.c_str());

  using Action = ShutterMotionPlan::Action;
  const uint8_t up_code = static_cast<uint8_t>(Command::UP);
  const uint8_t down_code = static_cast<uint8_t>(Command::DOWN);
  const uint8_t stop_code = static_cast<uint8_t>(Command::STOP);

  switch (plan.action) {
    case Action::None:
      ESP_LOGW(TAG, "set_position channel=%u target=%d -> no action (%s)", static_cast<unsigned>(channel_id), plan.target_pct, plan.info);
      return;
    case Action::Stop:
      if (!this->send_command(channel_id, stop_code)) {
        ESP_LOGW(TAG, "set_position channel=%u target=%d -> STOP send failed", static_cast<unsigned>(channel_id), plan.target_pct);
      }
      return;
    case Action::UpToEnd:
      if (!this->send_command(channel_id, up_code)) {
        ESP_LOGW(TAG, "set_position channel=%u target=%d -> UP send failed", static_cast<unsigned>(channel_id), plan.target_pct);
      }
      return;
    case Action::DownToEnd:
      if (!this->send_command(channel_id, down_code)) {
        ESP_LOGW(TAG, "set_position channel=%u target=%d -> DOWN send failed", static_cast<unsigned>(channel_id), plan.target_pct);
      }
      return;
    case Action::UpThenStop:
    case Action::DownThenStop: {
      const bool going_up = (plan.action == Action::UpThenStop);
      if (!this->send_command(channel_id, going_up ? up_code : down_code, false)) {
        ESP_LOGW(TAG, "set_position channel=%u target=%d -> initial move send failed", static_cast<unsigned>(channel_id), plan.target_pct);
        return;
      }
      this->arm_position_interpolation_(channel_id, current_pct, plan.target_pct, plan.duration_ms);
      const auto timer_name = this->mid_position_timer_name_(channel_id);
      const int captured_target = plan.target_pct;
      App.scheduler.set_timeout(this, timer_name.c_str(), plan.duration_ms, [this, channel_id, captured_target]() {
        this->cancel_position_interpolation_(channel_id);
        if (!this->send_command(channel_id, static_cast<uint8_t>(Command::STOP), false)) {
          ESP_LOGW(TAG, "set_position channel=%u -> timed STOP send failed", static_cast<unsigned>(channel_id));
          return;
        }
        if (channel_id < this->motor_pct_.size()) {
          this->motor_pct_[channel_id] = captured_target;
          this->refresh_covers_();
        }
      });
      return;
    }
  }
}

}  // namespace esphome::rojaflex
