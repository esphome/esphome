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
  if (this->state_ == EPaperState::RESET)
    this->step_ = Step::RESET_HIGH;

  switch (this->step_) {
    case Step::RESET_HIGH:
      this->reset_pin_->digital_write(true);
      this->reset_duration_ = RESET_SETTLE_MS;
      this->step_ = Step::RESET_LOW;
      return false;
    case Step::RESET_LOW:
      this->reset_pin_->digital_write(false);
      delay(2);
      this->reset_pin_->digital_write(true);
      this->reset_duration_ = RESET_SETTLE_MS;
      this->step_ = Step::RESET_SETTLE;
      return false;
    case Step::RESET_SETTLE:
      this->step_ = Step::INIT_SEQUENCE;
      // The panel stays busy after reset; start the init sequence without waiting for idle
      this->skip_next_idle_wait_();
      return true;
    default:
      this->mark_failed(LOG_STR("Invalid reset state"));
      return true;
  }
}

bool EPaperWaveshare3P97InG::initialise([[maybe_unused]] bool partial) {
  switch (this->step_) {
    case Step::INIT_SEQUENCE:
      this->send_init_sequence_(this->init_sequence_, this->init_sequence_length_);
      this->delay_until_ = millis() + 100;
      this->wait_for_idle_(true);
      this->step_ = Step::POWER_ON;
      return false;
    case Step::POWER_ON:
      this->command(0x04);
      this->delay_until_ = millis() + 100;
      this->wait_for_idle_(true);
      this->step_ = Step::INIT_DONE;
      return false;
    case Step::INIT_DONE:
      this->step_ = Step::NONE;
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
