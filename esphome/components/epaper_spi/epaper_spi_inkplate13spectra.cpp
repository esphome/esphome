#include "epaper_spi_inkplate13spectra.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi.inkplate13spectra");

void EPaperInkplate13Spectra::setup() {
  EPaperDualCS::setup();
  this->bs0_pin_->setup();
  this->bs1_pin_->setup();
  // EPaperDualCS::setup() leaves CS and CS1 high, and their pull-ups go to the panel supply, so
  // until the first update the unpowered panel would be fed through them. Hold every pin low
  // instead; on some boards that half-powered state stopped the panel from responding.
  this->set_all_pins_low_();
}

void EPaperInkplate13Spectra::setup_pins() const {
  for (auto *pin : this->enable_pins_)
    pin->setup();
  this->reset_pin_->setup();
  this->dc_pin_->setup();
  this->busy_pin_->setup();
}

void EPaperInkplate13Spectra::dump_config() {
  EPaperDualCS::dump_config();
  for (auto *pin : this->enable_pins_)
    LOG_PIN("  Enable Pin: ", pin);
  LOG_PIN("  BS0 Pin: ", this->bs0_pin_);
  LOG_PIN("  BS1 Pin: ", this->bs1_pin_);
}

void EPaperInkplate13Spectra::set_all_pins_low_() const {
  ESP_LOGV(TAG, "set_all_pins_low_()");
  // The busy pin is driven too, as Soldered's own driver does, so it cannot hold charge either.
  // Panel power goes off first, so the busy output never drives against a powered panel.
  auto drive_low = [](GPIOPin *p) {
    p->pin_mode(gpio::FLAG_OUTPUT);
    // The busy pin is inverted (active low), so a plain digital_write(false) would drive it
    // physically HIGH. is_inverted() as the write value lands on physical LOW either way. Only
    // internal pins expose is_inverted(); the model rejects inverted pins of any other kind.
    bool inverted = p->is_internal() && static_cast<InternalGPIOPin *>(p)->is_inverted();
    p->digital_write(inverted);
  };
  for (auto *pin : this->enable_pins_)
    drive_low(pin);
  for (auto *pin : {this->reset_pin_, this->dc_pin_, this->cs_pins_[0], this->cs_pins_[1], this->busy_pin_,
                    this->bs0_pin_, this->bs1_pin_})
    drive_low(pin);
}

void EPaperInkplate13Spectra::set_io_pins_() {
  ESP_LOGV(TAG, "set_io_pins_()");
  this->reset_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->dc_pin_->pin_mode(gpio::FLAG_OUTPUT);
  for (auto *cs : this->cs_pins_)
    cs->pin_mode(gpio::FLAG_OUTPUT);
  this->busy_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  for (auto *pin : this->enable_pins_)
    pin->pin_mode(gpio::FLAG_OUTPUT);
  this->bs0_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->bs1_pin_->pin_mode(gpio::FLAG_OUTPUT);

  this->dc_pin_->digital_write(true);
  for (auto *cs : this->cs_pins_)
    cs->digital_write(true);
  this->reset_pin_->digital_write(false);
  for (auto *pin : this->enable_pins_)
    pin->digital_write(false);
  this->bs0_pin_->digital_write(false);
  this->bs1_pin_->digital_write(true);
}

// Each step sets reset_duration_ to the time it needs; EPaperBase calls reset() again after
// that many ms. The model rejects a configured reset_duration, since it would be overwritten.
bool EPaperInkplate13Spectra::reset() {
  // EPaperBase calls reset() in RESET exactly once at the start of every cycle, so restart the
  // bring-up there. Doing it here rather than in deep_sleep() also covers a cycle that ended
  // without reaching deep_sleep(), such as one abandoned after a busy-pin timeout.
  if (this->state_ == EPaperState::RESET)
    this->reset_sub_ = RST_PINS_LOW;
  switch (this->reset_sub_) {
    case RST_PINS_LOW:
      ESP_LOGV(TAG, "reset(): RST_PINS_LOW");
      this->set_all_pins_low_();
      this->reset_duration_ = 500;
      this->reset_sub_ = RST_PINS_LOW_WAIT;
      return false;

    case RST_PINS_LOW_WAIT:
      ESP_LOGV(TAG, "reset(): RST_PINS_LOW_WAIT done -> set_io_pins_() + PWR_EN high");
      this->set_io_pins_();
      for (auto *pin : this->enable_pins_)
        pin->digital_write(true);
      this->reset_duration_ = 100;
      this->reset_sub_ = RST_IO_WAIT;
      return false;

    case RST_IO_WAIT:
      ESP_LOGV(TAG, "reset(): RST_IO_WAIT done -> RST low");
      this->reset_pin_->digital_write(false);
      this->reset_duration_ = 100;
      this->reset_sub_ = RST_LOW_WAIT;
      return false;

    case RST_LOW_WAIT:
      ESP_LOGV(TAG, "reset(): RST_LOW_WAIT done -> RST high");
      this->reset_pin_->digital_write(true);
      this->reset_duration_ = 100;
      this->reset_sub_ = RST_HIGH_WAIT;
      return false;

    case RST_HIGH_WAIT:
      ESP_LOGV(TAG, "reset(): RST_HIGH_WAIT done -> panel powered, entering INITIALISE");
      this->reset_sub_ = RST_DONE;
      return true;

    case RST_DONE:
      return true;
  }
  return false;
}

// Sends data byte 0x00 with the refresh command, as Soldered's own driver does.
void EPaperInkplate13Spectra::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "refresh_screen(): sending DRF");
  this->write_command_to_chip_(REG_DRF, {0x00}, CHIP_BOTH);
  this->refresh_started_ms_ = App.get_loop_component_start_time();
  // How long a partial refresh takes is not known, so only a full refresh is checked.
  this->refresh_pending_ = !partial;
}

uint32_t EPaperInkplate13Spectra::refresh_elapsed_ms_() const {
  return App.get_loop_component_start_time() - this->refresh_started_ms_;
}

bool EPaperInkplate13Spectra::refresh_was_ignored_() const {
  // A 6-colour refresh of this panel takes about 20 s, so anything under a second means the
  // panel did not run it.
  static constexpr uint32_t MIN_REFRESH_MS = 1000;
  return this->refresh_pending_ && this->refresh_elapsed_ms_() < MIN_REFRESH_MS;
}

// EPaperBase calls this once the busy pin has cleared after the refresh command.
bool EPaperInkplate13Spectra::after_refresh() {
  if (this->refresh_was_ignored_()) {
    ESP_LOGW(TAG,
             "Busy pin cleared %" PRIu32 " ms after the refresh command, but a refresh takes about 20 s. "
             "The panel most likely ignored the command; check its power supply and reset line.",
             this->refresh_elapsed_ms_());
  }
  this->refresh_pending_ = false;
  return true;
}

// Unlike the T133A01, no deep sleep command is sent: Soldered's driver only releases the pins.
void EPaperInkplate13Spectra::deep_sleep() {
  this->dc_pin_->pin_mode(gpio::FLAG_INPUT);
  for (auto *cs : this->cs_pins_)
    cs->pin_mode(gpio::FLAG_INPUT);
  // Keep the pullup so this pin reads a defined "idle" level at rest, since reset()
  // relies on that before it powers the panel back on next cycle.
  this->busy_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  for (auto *pin : this->enable_pins_) {
    pin->digital_write(false);
    pin->pin_mode(gpio::FLAG_INPUT);
  }

  // Held low, not released to an input, so the panel stays in its lowest-power state.
  this->reset_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->reset_pin_->digital_write(false);
  ESP_LOGV(TAG, "panel deep sleep");
}

}  // namespace esphome::epaper_spi
