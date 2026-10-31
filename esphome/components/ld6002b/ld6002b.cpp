#include "ld6002b.h"
#include "esphome/core/log.h"
#include <cinttypes>
#include <cstring>

namespace esphome::ld6002b {

ESPHOME_LOG_TAG(TAG, "ld6002b");

static constexpr uint16_t TYPE_REPORT_LOW_POWER = 0x0A12;
static constexpr uint16_t TYPE_REPORT_WORK_MODE = 0x0A14;

static constexpr uint32_t CMD_LOW_POWER_ON = 0x16;
static constexpr uint32_t CMD_LOW_POWER_OFF = 0x17;
static constexpr uint32_t CMD_GET_LOW_POWER = 0x18;

void LD6002BComponent::setup() {
  this->LD600XComponent::setup();
  if (this->is_failed())
    return;
  if (this->wakeup_pin_ != nullptr) {
    this->wakeup_pin_->setup();
    this->wakeup_pin_->digital_write(true);
  }
}

void LD6002BComponent::setup_model() {
#ifdef USE_SWITCH
  bool want_low_power = this->low_power_switch_ != nullptr;
  if (want_low_power) {
    // The query below confirms what the module took; until it lands, the work mode
    // fallback runs on the restored value.
    this->low_power_switch_->restore(false);
  }
#else
  bool want_low_power = false;
#endif
#ifdef USE_TEXT_SENSOR
  want_low_power = want_low_power || this->work_mode_text_sensor_ != nullptr;
#endif
  if (want_low_power) {
    this->send_control_command_(CMD_GET_LOW_POWER);
  }
}

void LD6002BComponent::dump_model_config() {
  ESP_LOGCONFIG(TAG, "  Auto wake: %s", this->auto_wake_ ? LOG_STR_LITERAL("true") : LOG_STR_LITERAL("false"));
  if (this->wakeup_pin_ != nullptr) {
    LOG_PIN("  Wake-up Pin: ", this->wakeup_pin_);
    ESP_LOGCONFIG(TAG, "  Wake Pulse: %" PRIu32 "ms", this->wakeup_pulse_ms_);
  }
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "Low Power", this->low_power_switch_);
#endif
}

bool LD6002BComponent::model_wants_target_stream() const {
#ifdef USE_TEXT_SENSOR
  // The work mode fallback reads presence off this stream. It weighs no target
  // presence at all while the stream is off.
  return this->work_mode_text_sensor_ != nullptr;
#else
  return false;
#endif
}

bool LD6002BComponent::handle_model_report(uint16_t type, const uint8_t *data, uint16_t len) {
  switch (type) {
    case TYPE_REPORT_LOW_POWER:
      this->handle_low_power_report_(data, len);
      return true;
    case TYPE_REPORT_WORK_MODE:
      this->handle_work_mode_report_(data, len);
      return true;
    default:
      return false;
  }
}

void LD6002BComponent::handle_low_power_report_(const uint8_t *data, uint16_t len) {
  if (len < 1)
    return;
  bool enabled = data[0] != 0;
  this->low_power_enabled_ = enabled;
  this->low_power_reported_ = true;
#ifdef USE_SWITCH
  PUBLISH_STATE(this->low_power_switch_, enabled);
#endif
  this->update_work_mode_fallback_();
}

void LD6002BComponent::handle_work_mode_report_(const uint8_t *data, uint16_t len) {
  if (len < 1)
    return;
  // Zero is the unattended half of this transition.  Read outside the text sensor's
  // ifdef because the area sensors do not need one configured to have gone stale.
  const bool low_power = (data[0] == 0);
#ifdef USE_TEXT_SENSOR
  if (this->work_mode_text_sensor_ != nullptr) {
    this->work_mode_reported_ = true;
    this->publish_work_mode_(low_power);
  }
#endif
  // Protocol V1.2 section 2.1.17: this message is sent only on the transition
  // between the unattended low-power mode and normal operation, so a zero is the
  // module stating that nobody is in any area.  Not while a target is still being
  // tracked, though: the reset_unattended command is undocumented on whether it
  // forces this report, and where two statements from the module disagree the live
  // one wins.
  if (low_power && !this->target_presence_any_) {
    this->clear_area_presence_();
  }
}

void LD6002BComponent::update_work_mode_fallback_() {
#ifdef USE_TEXT_SENSOR
  if (this->work_mode_text_sensor_ == nullptr || this->work_mode_reported_ || !this->low_power_reported_) {
    return;
  }
  // Target presence is only meaningful while the stream that maintains it runs.
  // Area presence keeps its own report, so it still counts with the target stream
  // off and low power alone decides only when neither half has anything to say.
  const bool presence = (this->target_display_enabled_ && this->target_presence_any_) || this->area_presence_any_;
  this->publish_work_mode_(this->low_power_enabled_ && !presence);
#endif
}

void LD6002BComponent::publish_work_mode_(bool low_power) {
#ifdef USE_TEXT_SENSOR
  if (this->work_mode_text_sensor_ == nullptr) {
    return;
  }
  if (this->work_mode_low_power_.next(low_power)) {
    this->work_mode_text_sensor_->publish_state(low_power ? "low_power" : "normal");
  }
#endif
}

bool LD6002BComponent::defer_frame_write(uint16_t type, const uint8_t *data, uint8_t len, bool track) {
  // Anonymous timeouts never replace each other; with a pulse already pending the module is waking anyway.
  if (!this->auto_wake_ || this->wakeup_pin_ == nullptr || this->wake_pulse_pending_)
    return false;
  // Snapshot the payload: the deferred write must not depend on state a completing command changes.
  if (len > 0 && data != nullptr) {
    std::memcpy(this->wake_scratch_.data(), data, len);
  }
  // A button pulse must not raise the pin in the middle of this one.
  this->cancel_timeout(WAKE_BUTTON_TIMEOUT_ID);
  this->wake_pulse_pending_ = true;
  this->wakeup_pin_->digital_write(false);
  const uint8_t generation = this->send_generation_;
  this->set_timeout(this->wakeup_pulse_ms_, [this, type, len, track, generation]() {
    this->wakeup_pin_->digital_write(true);
    this->wake_pulse_pending_ = false;
    // Anonymous timeouts are never cancelled, so a tracked pulse whose command has since been
    // retired must not transmit: the frame would land after its successor and be booked to it.
    if (track && generation != this->send_generation_) {
      return;
    }
    this->write_frame_(type, (len > 0) ? this->wake_scratch_.data() : nullptr, len, track);
  });
  return true;
}

void LD6002BComponent::wake_() {
  // A command's own pulse raises the pin and writes after it, so ride along instead of
  // claiming the flag: claiming it would send that command down the immediate-write path
  // with the pin still low.
  if (this->wakeup_pin_ == nullptr || this->wake_pulse_pending_)
    return;
  this->wakeup_pin_->digital_write(false);
  this->set_timeout(WAKE_BUTTON_TIMEOUT_ID, this->wakeup_pulse_ms_,
                    [this]() { this->wakeup_pin_->digital_write(true); });
}

#ifdef USE_SWITCH
void LD6002BComponent::set_switch_state(switch_::Switch *entity, bool state) {
  if (entity != this->low_power_switch_) {
    this->LD600XComponent::set_switch_state(entity, state);
    return;
  }
  this->low_power_enabled_ = state;
  this->low_power_reported_ = true;
  this->send_control_command_(state ? CMD_LOW_POWER_ON : CMD_LOW_POWER_OFF);
  this->update_work_mode_fallback_();
}
#endif

#ifdef USE_BUTTON
void LD6002BComponent::press_button(uint8_t kind) {
  if (kind == BUTTON_WAKE) {
    this->wake_();
  } else {
    this->LD600XComponent::press_button(kind);
  }
}
#endif

}  // namespace esphome::ld6002b
