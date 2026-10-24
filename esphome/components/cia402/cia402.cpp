#include "cia402.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::cia402 {

ESPHOME_LOG_TAG(TAG, "cia402");

void CiA402Drive::setup() {
  this->setup_canopen();
  this->add_on_emcy_callback([this](const canopen::Emergency &emergency) {
    if (emergency.error_code != 0 && this->running_)
      this->fail_(DriveResult::DRIVE_RESULT_FAULT);
  });
  this->update();
}

void CiA402Drive::dump_config() {
  ESP_LOGCONFIG(TAG, "CiA 402 node %u", this->node_);
  LOG_UPDATE_INTERVAL(this);
}

bool CiA402Drive::operational_(uint32_t now) const {
  return this->heartbeat_seen_ &&
         static_cast<int32_t>(now - this->last_heartbeat_ms_) < static_cast<int32_t>(this->heartbeat_timeout_ms_) &&
         this->get_nmt_state() == 5;
}

bool CiA402Drive::begin_() {
  if (this->busy() || !this->operational_(millis()) || this->is_failed()) {
    this->result_callback_.call(DriveResult::DRIVE_RESULT_REJECTED);
    return false;
  }
  this->step_count_ = 0;
  this->step_index_ = 0;
  this->resetting_ = false;
  this->running_ = true;
  this->started_ms_ = millis();
  this->enable_loop();
  return true;
}

void CiA402Drive::append_(const Step &step) {
  if (this->step_count_ < MAX_STEPS) {
    this->steps_[this->step_count_++] = step;
  } else {
    this->fail_(DriveResult::DRIVE_RESULT_REJECTED);
  }
}

void CiA402Drive::write_(uint16_t index, uint32_t value, uint8_t size, uint8_t subindex) {
  this->append_({value, 0, index, subindex, size, StepKind::STEP_KIND_WRITE});
}

void CiA402Drive::read_(uint16_t index, uint8_t size, uint32_t mask, uint32_t value, bool wait) {
  this->append_({value, mask, index, 0, size,
                 wait ? StepKind::STEP_KIND_WAIT : (mask ? StepKind::STEP_KIND_CHECK : StepKind::STEP_KIND_READ)});
}

bool CiA402Drive::enable() {
  if (!this->begin_())
    return false;
  this->read_(0x6041, 2, 8, 0);
  this->write_(0x6040, 6, 2);
  this->read_(0x6041, 2, 0x6F, 0x21, true);
  this->write_(0x6040, 7, 2);
  this->read_(0x6041, 2, 0x6F, 0x23, true);
  this->write_(0x6040, 15, 2);
  this->read_(0x6041, 2, 0x6F, 0x27, true);
  return true;
}

bool CiA402Drive::fault_reset() {
  if (!this->begin_())
    return false;
  this->resetting_ = true;
  this->write_(0x6040, 0, 2);
  this->write_(0x6040, 0x80, 2);
  this->read_(0x6041, 2, 8, 0, true);
  this->write_(0x6040, 0, 2);
  return true;
}

void CiA402Drive::append_profile_() {
  this->write_(0x6081, this->profile_velocity_, 4);
  this->write_(0x6083, this->acceleration_, 4);
  this->write_(0x6084, this->deceleration_, 4);
  this->write_(0x6085, this->deceleration_, 4);
}

void CiA402Drive::append_move_(int32_t position, bool relative) {
  this->read_(0x6041, 2, 0x6F, 0x27);
  this->read_(0x606C, 4, UINT32_MAX, 0);
  this->write_(0x6060, 1, 1);
  this->read_(0x6061, 1, 0xFF, 1, true);
  this->append_profile_();
  this->write_(0x6040, 15, 2);
  this->read_(0x6041, 2, 0x1000, 0, true);
  if (relative)
    this->append_({static_cast<uint32_t>(position), 0, 0x6064, 0, 4, StepKind::STEP_KIND_RELATIVE});
  this->write_(0x607A, static_cast<uint32_t>(position), 4);
  this->write_(0x6040, 0x3F, 2);
  this->read_(0x6041, 2, 0x1000, 0x1000, true);
  this->write_(0x6040, 15, 2);
  this->read_(0x6041, 2, 0x1400, 0x400, true);
  this->read_(0x606C, 4, UINT32_MAX, 0, true);
  this->read_(0x6064, 4);
}

bool CiA402Drive::move_to(int32_t position) {
  if (!this->begin_())
    return false;
  this->append_move_(position, false);
  return true;
}

bool CiA402Drive::move_by(int32_t distance) {
  if (!this->begin_())
    return false;
  this->append_move_(distance, true);
  return true;
}

void CiA402Drive::append_home_(int8_t method, int32_t offset) {
  this->read_(0x6041, 2, 0x6F, 0x27);
  this->read_(0x606C, 4, UINT32_MAX, 0);
  this->write_(0x6060, 6, 1);
  this->read_(0x6061, 1, 0xFF, 6, true);
  this->write_(0x6098, static_cast<uint8_t>(method), 1);
  this->write_(0x607C, static_cast<uint32_t>(offset), 4);
  this->write_(0x6099, this->profile_velocity_, 4, 1);
  this->write_(0x6099, this->profile_velocity_, 4, 2);
  this->write_(0x609A, this->acceleration_, 4);
  this->write_(0x6040, 15, 2);
  this->write_(0x6040, 31, 2);
  this->read_(0x6041, 2, 0x3400, 0x1400, true);
  this->write_(0x6040, 15, 2);
  this->read_(0x6064, 4);
}

bool CiA402Drive::home(int8_t method, int32_t offset) {
  if (!this->begin_())
    return false;
  this->append_home_(method, offset);
  return true;
}

bool CiA402Drive::disable() { return this->request_stop_(0); }
bool CiA402Drive::halt() { return this->request_stop_(0x10F); }
bool CiA402Drive::quick_stop() { return this->request_stop_(2); }

bool CiA402Drive::request_stop_(uint16_t controlword) {
  // Drain the single outstanding request; its reply must never advance the cancelled sequence.
  this->stopping_ = true;
  this->notify_stop_ = true;
  this->stop_controlword_ = controlword;
  if (this->running_)
    this->finish_(DriveResult::DRIVE_RESULT_CANCELLED);
  this->enable_loop();
  return true;
}

void CiA402Drive::finish_(DriveResult result) {
  this->running_ = false;
  this->step_count_ = 0;
  this->step_index_ = 0;
  this->resetting_ = false;
  if (this->sequence_finished_(result))
    this->result_callback_.call(result);
}

void CiA402Drive::fail_(DriveResult result) {
  this->status_set_warning();
  ESP_LOGW(TAG, "Node %u sequence failed: %u", this->node_, static_cast<unsigned>(result));
  this->stopping_ = true;
  this->notify_stop_ = false;
  this->stop_controlword_ = 2;
  this->finish_(result);
}

void CiA402Drive::update() {
  this->poll_due_ = true;
  this->enable_loop();
}

void CiA402Drive::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->running_ && (!this->operational_(now) || static_cast<int32_t>(now - this->started_ms_) >=
                                                         static_cast<int32_t>(this->operation_timeout_ms_)))
    this->fail_(DriveResult::DRIVE_RESULT_TIMEOUT);
  if (this->pending_)
    return;
  if (this->stopping_) {
    this->pending_stop_ = true;
    this->send_step_({this->stop_controlword_, 0, 0x6040, 0, 2, StepKind::STEP_KIND_WRITE});
    return;
  }
  if (this->running_) {
    if (this->step_index_ == this->step_count_) {
      this->status_clear_warning();
      this->finish_(DriveResult::DRIVE_RESULT_SUCCESS);
    } else if (now - this->last_request_ms_ >= 50) {
      this->pending_sequence_ = true;
      this->send_step_(this->steps_[this->step_index_]);
    }
    return;
  }
  if (this->poll_due_) {
    uint16_t index = 0x6041;
    uint8_t size = 2;
    switch (this->poll_index_) {
      case 1:
        index = 0x6061;
        size = 1;
        break;
      case 2:
        index = 0x6064;
        size = 4;
        break;
      case 3:
        index = 0x606C;
        size = 4;
        break;
    }
    this->send_step_({0, 0, index, 0, size, StepKind::STEP_KIND_READ});
    this->poll_index_ = (this->poll_index_ + 1) % 4;
    this->poll_due_ = this->poll_index_ != 0;
    return;
  }
  this->disable_loop();
}

void CiA402Drive::send_step_(const Step &step) {
  this->pending_step_ = step;
  this->pending_ = true;
  this->last_request_ms_ = App.get_loop_component_start_time();
  const bool accepted = step.kind == StepKind::STEP_KIND_WRITE
                            ? this->write_sdo(step.index, step.subindex, step.value, step.size, on_result, this)
                            : this->read_sdo(step.index, step.subindex, on_result, this);
  if (!accepted)
    this->process_result_(canopen::SDOResult::SDO_RESULT_REJECTED, 0, 0);
}

void CiA402Drive::on_result(void *context, canopen::SDOResult result, uint32_t value, uint8_t size) {
  static_cast<CiA402Drive *>(context)->process_result_(result, value, size);
}

void CiA402Drive::received_value_(uint16_t index, uint32_t value) {
  switch (index) {
    case 0x6041:
      this->statusword_ = value;
      break;
    case 0x6061:
      this->mode_ = sign_extend<8>(value);
      break;
    case 0x6064:
      this->position_ = sign_extend<32>(value);
      break;
    case 0x606C:
      this->velocity_ = sign_extend<32>(value);
      break;
  }
  this->value_callback_.call(index, value);
}

void CiA402Drive::process_result_(canopen::SDOResult result, uint32_t value, uint8_t size) {
  this->pending_ = false;
  const bool sequence = this->pending_sequence_;
  this->pending_sequence_ = false;
  const bool stop = this->pending_stop_;
  this->pending_stop_ = false;
  const bool success = result == canopen::SDOResult::SDO_RESULT_SUCCESS;
  if (stop) {
    this->stopping_ = false;
    if (!success)
      this->status_set_warning();
    if (this->notify_stop_)
      this->result_callback_.call(success ? DriveResult::DRIVE_RESULT_SUCCESS
                                          : DriveResult::DRIVE_RESULT_COMMUNICATION_ERROR);
    return;
  }
  if (this->stopping_ || (sequence && !this->running_))
    return;
  const auto &step = this->pending_step_;
  if (!success || (step.kind != StepKind::STEP_KIND_WRITE && size != step.size)) {
    this->status_set_warning();
    if (sequence)
      this->fail_(success ? DriveResult::DRIVE_RESULT_INVALID_RESPONSE : DriveResult::DRIVE_RESULT_COMMUNICATION_ERROR);
    return;
  }
  if (step.kind != StepKind::STEP_KIND_WRITE)
    this->received_value_(step.index, value);
  if (!sequence || !this->running_ || this->stopping_)
    return;
  if (!this->resetting_ && step.index == 0x6041 && ((value & 8) || (this->mode_ == 6 && (value & 0x2000)))) {
    this->fail_(DriveResult::DRIVE_RESULT_FAULT);
    return;
  }
  if ((step.kind == StepKind::STEP_KIND_CHECK || step.kind == StepKind::STEP_KIND_WAIT) &&
      (value & step.mask) != step.value) {
    if (step.kind == StepKind::STEP_KIND_CHECK)
      this->fail_(DriveResult::DRIVE_RESULT_REJECTED);
    return;
  }
  if (step.kind == StepKind::STEP_KIND_RELATIVE) {
    const int64_t target = static_cast<int64_t>(sign_extend<32>(value)) + sign_extend<32>(step.value);
    if (target < INT32_MIN || target > INT32_MAX) {
      this->fail_(DriveResult::DRIVE_RESULT_REJECTED);
      return;
    }
    this->steps_[this->step_index_ + 1].value = static_cast<uint32_t>(target);
  }
  this->step_index_++;
}

}  // namespace esphome::cia402
