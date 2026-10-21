#include "epaper_spi_inkplate13spectra.h"
#include "colorconv.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi.inkplate13spectra");

// Native hardware color codes for this panel's 4-bit color values.
enum Inkplate13SpectraColor : uint8_t {
  BLACK = 0x00,
  WHITE = 0x01,
  YELLOW = 0x02,
  RED = 0x03,
  BLUE = 0x05,
  GREEN = 0x06,
};

uint8_t EPaperInkplate13Spectra::color_to_native(Color color) {
  return color_to_bwyrgb<uint8_t>(color, BLACK, WHITE, YELLOW, RED, GREEN, BLUE);
}

void EPaperInkplate13Spectra::setup() {
  EPaperDualCS::setup();
  this->bs0_pin_->setup();
  this->bs1_pin_->setup();
  // EPaperDualCS::setup() leaves CS and CS1 high, and their pull-ups go to the panel supply, so
  // until the first update the unpowered panel would be fed through them. Hold every pin low
  // instead; on some boards that half-powered state stopped the panel from responding.
  this->set_all_pins_low_();
}

void EPaperInkplate13Spectra::setup_pins_() const {
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

void EPaperInkplate13Spectra::send_init_sequence_() {
  this->write_command_to_chip_(0x74, {0xC0, 0x1C, 0x1C, 0xCC, 0xCC, 0xCC, 0x15, 0x15, 0x55}, CHIP_PRIMARY);  // AN_TM
  this->write_command_to_chip_(REG_CMD66, CMD66_V, sizeof(CMD66_V), CHIP_BOTH);
  this->write_command_to_chip_(0x00, {0xDF, 0x6B}, CHIP_BOTH);                             // PSR
  this->write_command_to_chip_(0x30, {0x08}, CHIP_BOTH);                                   // PLL
  this->write_command_to_chip_(0x50, {0xF7}, CHIP_BOTH);                                   // CDI
  this->write_command_to_chip_(0x60, {0x03, 0x03}, CHIP_BOTH);                             // TCON
  this->write_command_to_chip_(0x86, {0x10}, CHIP_BOTH);                                   // AGID
  this->write_command_to_chip_(0xE3, {0x22}, CHIP_BOTH);                                   // PWS
  this->write_command_to_chip_(REG_CCSET, {0x01}, CHIP_BOTH);                              // CCSET
  this->write_command_to_chip_(0x61, {0x04, 0xB0, 0x03, 0x20}, CHIP_BOTH);                 // TRES
  this->write_command_to_chip_(0x01, {0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38}, CHIP_PRIMARY);  // PWR
  this->write_command_to_chip_(0xB6, {0x07}, CHIP_PRIMARY);                                // EN_BUF
  this->write_command_to_chip_(0x06, {0xD8, 0x18}, CHIP_PRIMARY);                          // BTST_P
  this->write_command_to_chip_(0xB7, {0x01}, CHIP_PRIMARY);                                // BOOST_VDDP_EN
  this->write_command_to_chip_(0x05, {0xD8, 0x18}, CHIP_PRIMARY);                          // BTST_N
  this->write_command_to_chip_(0xB0, {0x01}, CHIP_PRIMARY);                                // BUCK_BOOST_VDDN
  this->write_command_to_chip_(0xB1, {0x02}, CHIP_PRIMARY);                                // TFT_VCOM_POWER
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
  for (auto *pin : {this->reset_pin_, this->dc_pin_, this->cs_pin_, this->cs1_pin_, this->busy_pin_, this->bs0_pin_,
                    this->bs1_pin_})
    drive_low(pin);
}

void EPaperInkplate13Spectra::set_io_pins_() {
  ESP_LOGV(TAG, "set_io_pins_()");
  this->reset_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->dc_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->cs_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->cs1_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->busy_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  for (auto *pin : this->enable_pins_)
    pin->pin_mode(gpio::FLAG_OUTPUT);
  this->bs0_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->bs1_pin_->pin_mode(gpio::FLAG_OUTPUT);

  this->dc_pin_->digital_write(true);
  this->cs_pin_->digital_write(true);
  this->cs1_pin_->digital_write(true);
  this->reset_pin_->digital_write(false);
  for (auto *pin : this->enable_pins_)
    pin->digital_write(false);
  this->bs0_pin_->digital_write(false);
  this->bs1_pin_->digital_write(true);
}

// Runs entirely inside RESET_END, which EPaperBase never busy-gates (see header), so the
// panel is fully powered before any busy-pin read happens. Each step sets reset_duration_
// to the delay this stage needs; the framework re-enters RESET_END after that many ms. The
// model rejects a configured reset_duration, since it would be overwritten here.
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

// Runs after reset() has already powered the panel on, so is_idle_() here reflects a
// real, live signal instead of an unpowered floating/pulled line. PON is sent later, in
// power_on() -- the framework's own set_state_() already busy-waits between TRANSFER_DATA
// and POWER_ON, and again between POWER_ON and REFRESH_SCREEN, so sending PON in its
// standard slot (after the framebuffer transfer, before refresh) needs no extra bookkeeping
// here. Confirmed on real hardware that data-before-power-on works fine on this panel.
bool EPaperInkplate13Spectra::initialise(bool partial) {
  ESP_LOGV(TAG, "initialise(): sending register init sequence");
  this->send_init_sequence_();
  return true;
}

void EPaperInkplate13Spectra::power_on() {
  ESP_LOGV(TAG, "power_on(): sending PON");
  this->write_command_to_chip_(REG_PON, CHIP_BOTH);
}

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

// EPaperBase's automatic busy-wait before DEEP_SLEEP covers the wait for POF to complete.
void EPaperInkplate13Spectra::power_off() {
  ESP_LOGV(TAG, "power_off(): sending POF");
  this->write_command_to_chip_(REG_POF, {0x00}, CHIP_BOTH);
}

void EPaperInkplate13Spectra::deep_sleep() {
  this->dc_pin_->pin_mode(gpio::FLAG_INPUT);
  this->cs_pin_->pin_mode(gpio::FLAG_INPUT);
  this->cs1_pin_->pin_mode(gpio::FLAG_INPUT);
  // Keep the pullup so this pin reads a defined "idle" level at rest, since reset()
  // relies on that before it powers the panel back on next cycle.
  this->busy_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  for (auto *pin : this->enable_pins_) {
    pin->digital_write(false);
    pin->pin_mode(gpio::FLAG_INPUT);
  }

  // RST intentionally NOT released to input -- held low for hardware deep sleep (~uA draw).
  this->reset_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->reset_pin_->digital_write(false);
  ESP_LOGV(TAG, "panel deep sleep");
}

}  // namespace esphome::epaper_spi
