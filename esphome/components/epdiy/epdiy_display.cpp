#include "epdiy_display.h"

#include "esphome/core/application.h"

#ifdef USE_ESP_IDF

#include "esphome/core/log.h"

namespace esphome::epdiy {

static const char *const TAG = "epdiy";

static constexpr uint8_t TEMPERATURE = 23;  // default temperature for e-paper displays

float EPDiyDisplay::get_setup_priority() const { return esphome::setup_priority::LATE; }

void EPDiyDisplay::setup() {
  epd_init(this->board_definition_, this->display_t_, this->init_options_);
  if (this->vcom_mv_ != 0) {
    epd_set_vcom(this->vcom_mv_);
  }
  this->state_ = epd_hl_init(nullptr);
  this->framebuffer_ = epd_hl_get_framebuffer(&this->state_);
}

void EPDiyDisplay::update() {
  this->do_update_();
  this->defer([this]() { this->flush_screen_changes_(); });
}

void EPDiyDisplay::fill(Color color) {
  if (color == display::COLOR_OFF) {
    memset(this->framebuffer_, 0xFF, this->get_buffer_length());

    epd_poweron();
    epd_hl_update_screen(&this->state_, MODE_GC16, TEMPERATURE);
    epd_clear();

    epd_poweroff();
    App.feed_wdt();
  } else {
    Display::fill(color);
  }
}

void EPDiyDisplay::flush_screen_changes_() {
  epd_poweron();

  epd_hl_update_screen(&this->state_, MODE_GC16, TEMPERATURE);
  memset(this->state_.back_fb, 0xFF, this->get_buffer_length());

  uint16_t delay = 0;
  if (this->power_off_delay_enabled_) {
    delay = 700;
  }
  this->set_timeout("poweroff", delay, []() { epd_poweroff(); });
}

void EPDiyDisplay::on_shutdown() {
  epd_poweroff();
  epd_deinit();
}

void HOT EPDiyDisplay::draw_pixel_at(int x, int y, Color color) {
  if (color.red == 255 && color.green == 255 && color.blue == 255) {
    epd_draw_pixel(x, y, 0, this->framebuffer_);
  } else {
    int col = (0.2126 * color.red) + (0.7152 * color.green) + (0.0722 * color.blue);
    int cl = 255 - col;
    epd_draw_pixel(x, y, cl, this->framebuffer_);
  }
}

}  // namespace esphome::epdiy

#endif  // USE_ESP_IDF
