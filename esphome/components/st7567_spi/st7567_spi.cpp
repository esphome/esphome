#include "st7567_spi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7567_spi {

static const char *const TAG = "st7567_spi";

void SPIST7567::setup() {
    ESP_LOGCONFIG(TAG, "Setting up SPI ST7567 display...");
    this->spi_setup();
    this->dc_pin_->setup();
    if (this->cs_)
        this->cs_->setup();

    this->init_reset_();
    ST7567::setup();
}

void SPIST7567::dump_config() {
    LOG_DISPLAY("", "SPI ST7567", this);
    ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_());
    LOG_PIN("  CS Pin: ", this->cs_);
    LOG_PIN("  DC Pin: ", this->dc_pin_);
    LOG_PIN("  Reset Pin: ", this->reset_pin_);
    ESP_LOGCONFIG(TAG, "  Flip X: %s", YESNO(this->flip_x_));
    ESP_LOGCONFIG(TAG, "  Flip Y: %s", YESNO(this->flip_y_));
    ESP_LOGCONFIG(TAG, "  Inverted Color: %s", YESNO(this->invert_));
    LOG_UPDATE_INTERVAL(this);
}

void SPIST7567::command(uint8_t value) {
    if (this->cs_)
        this->cs_->digital_write(true);
    this->dc_pin_->digital_write(false);
    delay(1);
    this->enable();
    if (this->cs_)
        this->cs_->digital_write(false);
    this->write_byte(value);
    if (this->cs_)
        this->cs_->digital_write(true);
    this->disable();
}

void HOT SPIST7567::write_display_data() {
    ESP_LOGD(TAG, "write_display_data()");

    this->command(esphome::st7567_base::ST7567_SET_START_LINE);
    for (uint8_t y = 0; y < (uint8_t) this->get_height_internal() / 8; y++) {
        this->dc_pin_->digital_write(false);
        this->command(esphome::st7567_base::ST7567_PAGE_ADDR + y);  // Set Page
        this->command(esphome::st7567_base::ST7567_COL_ADDR_H);     // Set MSB Column address
        this->command(esphome::st7567_base::ST7567_COL_ADDR_L);     // Set LSB Column address
        this->dc_pin_->digital_write(true);

        this->enable();
        this->write_array(&this->buffer_[y * this->get_width_internal()], this->get_width_internal());
        this->disable();
    }
}

}  // namespace st7567_spi
}  // namespace esphome
