#include "ssd1306_spi.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace ssd1306_spi {

static const char *const TAG = "ssd1306_spi";

void SPISSD1306::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SPI SSD1306...");
  this->spi_setup();
  this->dc_pin_->setup();  // OUTPUT

  this->init_reset_();
  SSD1306::setup();
}
void SPISSD1306::dump_config() {
  LOG_DISPLAY("", "SPI SSD1306", this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_());
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  External VCC: %s", YESNO(this->external_vcc_));
  ESP_LOGCONFIG(TAG, "  Flip X: %s", YESNO(this->flip_x_));
  ESP_LOGCONFIG(TAG, "  Flip Y: %s", YESNO(this->flip_y_));
  ESP_LOGCONFIG(TAG, "  Offset X: %d", this->offset_x_);
  ESP_LOGCONFIG(TAG, "  Offset Y: %d", this->offset_y_);
  ESP_LOGCONFIG(TAG, "  Inverted Color: %s", YESNO(this->invert_));
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
    for (uint8_t y = 0; y < (uint8_t) this->get_height_internal() / 8; y++) {
      this->command(0xB0 + y);
      if (this->is_sh1106_()) {
        this->command(0x02);
      } else {
        this->command(0x00);
      }
      this->command(0x10);
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

}  // namespace ssd1306_spi
}  // namespace esphome
