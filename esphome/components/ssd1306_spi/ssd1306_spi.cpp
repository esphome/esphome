#include "ssd1306_spi.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::ssd1306_spi {

static const char *const TAG = "ssd1306_spi";

void SPISSD1306::setup() {
  this->spi_setup();
  this->dc_pin_->setup();  // OUTPUT

  this->init_reset_();
  SSD1306::setup();
}
void SPISSD1306::dump_config() {
  LOG_DISPLAY("", "SPI SSD1306", this);
  ESP_LOGCONFIG(TAG,
                "  Model: %s\n"
                "  External VCC: %s\n"
                "  Flip X: %s\n"
                "  Flip Y: %s\n"
                "  Offset X: %d\n"
                "  Offset Y: %d\n"
                "  Inverted Color: %s",
                LOG_STR_ARG(this->model_str_()), YESNO(this->external_vcc_), YESNO(this->flip_x_), YESNO(this->flip_y_),
                this->offset_x_, this->offset_y_, YESNO(this->invert_));
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);
}
void SPISSD1306::command(uint8_t value) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(value);
  this->disable();
}
void HOT SPISSD1306::write_display_data() {
  if (this->is_sh1106_() || this->is_sh1107_()) {
    // Some panels wire their visible columns to a window of the controller RAM
    // that does not start at column 0 (e.g. SH1107 M5Stack Unit OLED needs offset_x: 32).
    // SH1106 keeps its historical 0x02 base column on top of any offset.
    uint8_t start_column = this->offset_x_;
    if (this->is_sh1106_()) {
      start_column += 0x02;
    }
    for (uint8_t y = 0; y < (uint8_t) this->get_height_internal() / 8; y++) {
      this->command(0xB0 + y);
      this->command(start_column & 0x0F);         // lower column
      this->command(0x10 | (start_column >> 4));  // higher column
      this->dc_pin_->digital_write(true);
      for (uint8_t x = 0; x < (uint8_t) this->get_width_internal(); x++) {
        this->enable();
        this->write_byte(this->buffer_[x + y * this->get_width_internal()]);
        this->disable();
        App.feed_wdt();
      }
    }
  } else {
    this->dc_pin_->digital_write(true);
    this->enable();
    this->write_array(this->buffer_, this->get_buffer_length_());
    this->disable();
  }
}

}  // namespace esphome::ssd1306_spi
