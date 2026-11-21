#include "epaper_waveshare_3p97_g.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

void EPaperWaveshare3P97InG::fill(Color color) {
  EPaperJD79660::fill(color);
  // A clipped fill only covers part of the panel, so it must not mark the whole frame dirty
  if (!this->get_clipping().is_set())
    this->mark_all_drawn_();
}

bool EPaperWaveshare3P97InG::reset() {
  // The parent checks for idle after reset, but this panel is still busy until the init sequence has run
  if (this->state_ == EPaperState::RESET_END && this->step_ == FSMState::RESET_STEP2_IDLECHECK) {
    this->step_ = FSMState::INIT_STEP0_REGULARINIT;
    this->skip_next_idle_wait_();
    return true;
  }
  return EPaperJD79660::reset();
}

bool EPaperWaveshare3P97InG::initialise([[maybe_unused]] bool partial) {
  switch (this->step_) {
    case FSMState::INIT_STEP0_REGULARINIT:
      this->send_init_sequence_(this->init_sequence_, this->init_sequence_length_);
      this->delay_until_ = millis() + 100;
      this->wait_for_idle_(true);
      this->step_ = FSMState::INIT_STEP2_POWERON;
      return false;
    case FSMState::INIT_STEP2_POWERON:
      this->command(0x04);
      this->delay_until_ = millis() + 100;
      this->wait_for_idle_(true);
      this->step_ = FSMState::NONE;
      return false;
    case FSMState::NONE:
      // The power-on wait finished; the init sequence is complete
      return true;
    default:
      this->mark_failed(LOG_STR("Invalid initialise state"));
      return true;
  }
}

void EPaperWaveshare3P97InG::refresh_screen(bool partial) {
  EPaperJD79660::refresh_screen(partial);
  this->next_delay_ = 100;
}

void EPaperWaveshare3P97InG::power_off() {
  EPaperJD79660::power_off();
  // The panel stops driving BUSY once off, so don't wait before deep sleep
  this->skip_next_idle_wait_();
}

}  // namespace esphome::epaper_spi
