#include "l298n.h"
#include "esphome/core/log.h"

namespace esphome::l298n {

ESPHOME_LOG_TAG(TAG, "l298n.stepper");

static const LogString *step_mode_to_log_string(L298NStepMode mode) {
  switch (mode) {
    case L298N_STEP_MODE_FULL_STEP:
      return LOG_STR("FULL STEP");
    case L298N_STEP_MODE_HALF_STEP:
      return LOG_STR("HALF STEP");
    case L298N_STEP_MODE_WAVE_DRIVE:
      return LOG_STR("WAVE DRIVE");
    default:
      return LOG_STR("UNKNOWN");
  }
}

void L298N::setup() {
  this->pin_a_->setup();
  this->pin_b_->setup();
  this->pin_c_->setup();
  this->pin_d_->setup();
  this->loop();
}
void L298N::loop() {
  int dir = this->should_step_();
  if (dir == 0 && this->has_reached_target()) {
    this->high_freq_.stop();

    if (this->sleep_when_done_) {
      this->pin_a_->digital_write(false);
      this->pin_b_->digital_write(false);
      this->pin_c_->digital_write(false);
      this->pin_d_->digital_write(false);
      // do not write pos
      return;
    }
  } else {
    this->high_freq_.start();
    this->current_pos_ += dir;
  }

  this->write_step_(this->current_pos_);
}
void L298N::dump_config() {
  ESP_LOGCONFIG(TAG,
                "L298N:\n"
                "  Sleep when done: %s",
                YESNO(this->sleep_when_done_));
  LOG_PIN("  Pin A: ", this->pin_a_);
  LOG_PIN("  Pin B: ", this->pin_b_);
  LOG_PIN("  Pin C: ", this->pin_c_);
  LOG_PIN("  Pin D: ", this->pin_d_);
  ESP_LOGCONFIG(TAG, "  Step Mode: %s", LOG_STR_ARG(step_mode_to_log_string(this->step_mode_)));
}
void L298N::write_step_(int32_t step) {
  int32_t n = this->step_mode_ == L298N_STEP_MODE_HALF_STEP ? 8 : 4;
  auto i = static_cast<uint32_t>((step % n + n) % n);
  uint8_t res = 0;

  switch (this->step_mode_) {
    case L298N_STEP_MODE_FULL_STEP: {
      // AB, BC, CD, DA
      res |= 1 << i;
      res |= 1 << ((i + 1) % 4);
      break;
    }
    case L298N_STEP_MODE_HALF_STEP: {
      // A, AB, B, BC, C, CD, D, DA
      res |= 1 << (i >> 1);
      res |= 1 << (((i + 1) >> 1) & 0x3);
      break;
    }
    case L298N_STEP_MODE_WAVE_DRIVE: {
      // A, B, C, D
      res |= 1 << i;
      break;
    }
  }

  this->pin_a_->digital_write((res >> 0) & 1);
  this->pin_b_->digital_write((res >> 1) & 1);
  this->pin_c_->digital_write((res >> 2) & 1);
  this->pin_d_->digital_write((res >> 3) & 1);
}

}  // namespace esphome::l298n
