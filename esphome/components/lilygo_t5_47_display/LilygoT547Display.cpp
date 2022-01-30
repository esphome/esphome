#include "LilygoT547Display.h"
#include "esphome/core/log.h"

#define WAVEFORM EPD_BUILTIN_WAVEFORM

namespace esphome {
namespace lilygo_t5_47_display {

static const char *const TAG = "lilygo_t5_47_display";

float LilygoT547Display::get_setup_priority() const { return esphome::setup_priority::LATE; }

void LilygoT547Display::set_clear_screen(bool clear) { this->clear_ = clear; }
void LilygoT547Display::set_power_off_delay_enabled(bool power_off_delay_enabled) {
  this->power_off_delay_enabled_ = power_off_delay_enabled;
}
void LilygoT547Display::set_landscape(bool landscape) { this->landscape_ = landscape; }

void LilygoT547Display::set_temperature(uint32_t temperature) { this->temperature_ = temperature; }

int LilygoT547Display::get_width_internal() { return 960; }

int LilygoT547Display::get_height_internal() { return 540; }

void LilygoT547Display::setup() {
  epd_init(EPD_OPTIONS_DEFAULT);
  hl = epd_hl_init(WAVEFORM);
  if (landscape_) {
    EpdRotation orientation = EPD_ROT_LANDSCAPE;
    epd_set_rotation(orientation);
  } else {
    EpdRotation orientation = EPD_ROT_PORTRAIT;
    epd_set_rotation(orientation);
  }
  fb = epd_hl_get_framebuffer(&hl);
}

void LilygoT547Display::update() {
  if (this->init_clear_executed_ == false && this->clear_ == true) {
    LilygoT547Display::clear();
    this->init_clear_executed_ = true;
  }
  this->do_update_();
  LilygoT547Display::flush_screen_changes();
}

void LilygoT547Display::clear() {
  epd_poweron();
  epd_fullclear(&hl, this->temperature_);
  epd_poweroff();
}

void LilygoT547Display::flush_screen_changes() {
  epd_poweron();
  err = epd_hl_update_screen(&hl, MODE_GC16, this->temperature_);
  if (this->power_off_delay_enabled_ == true) {
    delay(700);
  }
  epd_poweroff();
}

void LilygoT547Display::set_all_white() { epd_hl_set_all_white(&hl); }
void LilygoT547Display::poweron() { epd_poweron(); }
void LilygoT547Display::poweroff() { epd_poweroff(); }

void LilygoT547Display::on_shutdown() {
  ESP_LOGI(TAG, "Shutting down Lilygo T5-4.7 screen");
  epd_poweroff();
  epd_deinit();
}

void HOT LilygoT547Display::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (color.red == 255 && color.green == 255 && color.blue == 255) {
    epd_draw_pixel(x, y, 0, fb);
  } else {
    int col = (0.2126 * color.red) + (0.7152 * color.green) + (0.0722 * color.blue);
    int cl = 255 - col;
    epd_draw_pixel(x, y, cl, fb);
  }
}

}  // namespace lilygo_t5_47_display
}  // namespace esphome
