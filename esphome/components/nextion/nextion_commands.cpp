#include "nextion.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nextion {
static const char *const TAG = "nextion";

// Sleep safe commands
void Nextion::soft_reset() { this->send_command_("rest"); }

void Nextion::set_wake_up_page(uint8_t page_id) {
  if (page_id > 255) {
    ESP_LOGD(TAG, "Wake up page of bounds, range 0-255");
    return;
  }
  this->add_no_result_to_queue_with_set_internal_("wake_up_page", "wup", page_id, true);
}

void Nextion::set_touch_sleep_timeout(uint16_t timeout) {
  if (timeout < 3 || timeout > 65535) {
    ESP_LOGD(TAG, "Sleep timeout out of bounds, range 3-65535");
    return;
  }

  this->add_no_result_to_queue_with_set_internal_("touch_sleep_timeout", "thsp", timeout, true);
}

void Nextion::sleep(bool sleep) {
  if (sleep) {  // Set sleep
    this->is_sleeping_ = true;
    this->add_no_result_to_queue_with_set_internal_("sleep", "sleep", 1, true);
  } else {  // Turn off sleep. Wait for a sleep_wake return before setting sleep off
    this->add_no_result_to_queue_with_set_internal_("sleep_wake", "sleep", 0, true);
  }
}
// End sleep safe commands

// Set Colors
void Nextion::set_component_background_color(const char *component, uint32_t color) {
  this->add_no_result_to_queue_with_printf_("set_component_background_color", "%s.bco=%d", component, color);
}

void Nextion::set_component_background_color(const char *component, const char *color) {
  this->add_no_result_to_queue_with_printf_("set_component_background_color", "%s.bco=%s", component, color);
}

void Nextion::set_component_background_color(const char *component, Color color) {
  this->add_no_result_to_queue_with_printf_("set_component_background_color", "%s.bco=%d", component,
                                            display::ColorUtil::color_to_565(color));
}

void Nextion::set_component_pressed_background_color(const char *component, uint32_t color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_background_color", "%s.bco2=%d", component, color);
}

void Nextion::set_component_pressed_background_color(const char *component, const char *color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_background_color", "%s.bco2=%s", component, color);
}

void Nextion::set_component_pressed_background_color(const char *component, Color color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_background_color", "%s.bco2=%d", component,
                                            display::ColorUtil::color_to_565(color));
}

void Nextion::set_component_pic(const char *component, uint8_t pic_id) {
  this->add_no_result_to_queue_with_printf_("set_component_pic", "%s.pic=%d", component, pic_id);
}

void Nextion::set_component_picc(const char *component, uint8_t pic_id) {
  this->add_no_result_to_queue_with_printf_("set_component_pic", "%s.picc=%d", component, pic_id);
}

void Nextion::set_component_font_color(const char *component, uint32_t color) {
  this->add_no_result_to_queue_with_printf_("set_component_font_color", "%s.pco=%d", component, color);
}

void Nextion::set_component_font_color(const char *component, const char *color) {
  this->add_no_result_to_queue_with_printf_("set_component_font_color", "%s.pco=%s", component, color);
}

void Nextion::set_component_font_color(const char *component, Color color) {
  this->add_no_result_to_queue_with_printf_("set_component_font_color", "%s.pco=%d", component,
                                            display::ColorUtil::color_to_565(color));
}

void Nextion::set_component_pressed_font_color(const char *component, uint32_t color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_font_color", "%s.pco2=%d", component, color);
}

void Nextion::set_component_pressed_font_color(const char *component, const char *color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_font_color", " %s.pco2=%s", component, color);
}

void Nextion::set_component_pressed_font_color(const char *component, Color color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_font_color", "%s.pco2=%d", component,
                                            display::ColorUtil::color_to_565(color));
}

void Nextion::set_component_text_printf(const char *component, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[256];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    this->set_component_text(component, buffer);
}

// General Nextion
void Nextion::goto_page(const char *page) { this->add_no_result_to_queue_with_printf_("goto_page", "page %s", page); }

void Nextion::set_backlight_brightness(float brightness) {
  if (brightness < 0 || brightness > 1.0) {
    ESP_LOGD(TAG, "Brightness out of bounds, percentage range 0-1.0");
    return;
  }
  this->add_no_result_to_queue_with_set("backlight_brightness", "dim", static_cast<int>(brightness * 100));
}

void Nextion::set_auto_wake_on_touch(bool auto_wake) {
  this->add_no_result_to_queue_with_set("auto_wake_on_touch", "thup", auto_wake ? 1 : 0);
}

// General Component
void Nextion::set_component_font(const char *component, uint8_t font_id) {
  this->add_no_result_to_queue_with_printf_("set_component_font", "%s.font=%d", component, font_id);
}

void Nextion::hide_component(const char *component) {
  this->add_no_result_to_queue_with_printf_("hide_component", "vis %s,0", component);
}

void Nextion::show_component(const char *component) {
  this->add_no_result_to_queue_with_printf_("show_component", "vis %s,1", component);
}

void Nextion::enable_component_touch(const char *component) {
  this->add_no_result_to_queue_with_printf_("enable_component_touch", "tsw %s,1", component);
}

void Nextion::disable_component_touch(const char *component) {
  this->add_no_result_to_queue_with_printf_("disable_component_touch", "tsw %s,0", component);
}

void Nextion::set_component_picture(const char *component, const char *picture) {
  this->add_no_result_to_queue_with_printf_("set_component_picture", "%s.val=%s", component, picture);
}

void Nextion::set_component_text(const char *component, const char *text) {
  this->add_no_result_to_queue_with_printf_("set_component_text", "%s.txt=\"%s\"", component, text);
}

void Nextion::set_component_value(const char *component, int value) {
  this->add_no_result_to_queue_with_printf_("set_component_value", "%s.val=%d", component, value);
}

void Nextion::add_waveform_data(int component_id, uint8_t channel_number, uint8_t value) {
  this->add_no_result_to_queue_with_printf_("add_waveform_data", "add %d,%u,%u", component_id, channel_number, value);
}

void Nextion::open_waveform_channel(int component_id, uint8_t channel_number, uint8_t value) {
  this->add_no_result_to_queue_with_printf_("open_waveform_channel", "addt %d,%u,%u", component_id, channel_number,
                                            value);
}

void Nextion::set_component_coordinates(const char *component, int x, int y) {
  this->add_no_result_to_queue_with_printf_("set_component_coordinates command 1", "%s.xcen=%d", component, x);
  this->add_no_result_to_queue_with_printf_("set_component_coordinates command 2", "%s.ycen=%d", component, y);
}

// Drawing
void Nextion::display_picture(int picture_id, int x_start, int y_start) {
  this->add_no_result_to_queue_with_printf_("display_picture", "pic %d %d %d", x_start, y_start, picture_id);
}

void Nextion::fill_area(int x1, int y1, int width, int height, const char *color) {
  this->add_no_result_to_queue_with_printf_("fill_area", "fill %d,%d,%d,%d,%s", x1, y1, width, height, color);
}

void Nextion::fill_area(int x1, int y1, int width, int height, Color color) {
  this->add_no_result_to_queue_with_printf_("fill_area", "fill %d,%d,%d,%d,%d", x1, y1, width, height,
                                            display::ColorUtil::color_to_565(color));
}

void Nextion::line(int x1, int y1, int x2, int y2, const char *color) {
  this->add_no_result_to_queue_with_printf_("line", "line %d,%d,%d,%d,%s", x1, y1, x2, y2, color);
}

void Nextion::line(int x1, int y1, int x2, int y2, Color color) {
  this->add_no_result_to_queue_with_printf_("line", "line %d,%d,%d,%d,%d", x1, y1, x2, y2,
                                            display::ColorUtil::color_to_565(color));
}

void Nextion::rectangle(int x1, int y1, int width, int height, const char *color) {
  this->add_no_result_to_queue_with_printf_("draw", "draw %d,%d,%d,%d,%s", x1, y1, x1 + width, y1 + height, color);
}

void Nextion::rectangle(int x1, int y1, int width, int height, Color color) {
  this->add_no_result_to_queue_with_printf_("draw", "draw %d,%d,%d,%d,%d", x1, y1, x1 + width, y1 + height,
                                            display::ColorUtil::color_to_565(color));
}

void Nextion::circle(int center_x, int center_y, int radius, const char *color) {
  this->add_no_result_to_queue_with_printf_("cir", "cir %d,%d,%d,%s", center_x, center_y, radius, color);
}

void Nextion::circle(int center_x, int center_y, int radius, Color color) {
  this->add_no_result_to_queue_with_printf_("cir", "cir %d,%d,%d,%d", center_x, center_y, radius,
                                            display::ColorUtil::color_to_565(color));
}

void Nextion::filled_circle(int center_x, int center_y, int radius, const char *color) {
  this->add_no_result_to_queue_with_printf_("cirs", "cirs %d,%d,%d,%s", center_x, center_y, radius, color);
}

void Nextion::filled_circle(int center_x, int center_y, int radius, Color color) {
  this->add_no_result_to_queue_with_printf_("cirs", "cirs %d,%d,%d,%d", center_x, center_y, radius,
                                            display::ColorUtil::color_to_565(color));
}

#ifdef USE_TIME
void Nextion::set_nextion_rtc_time(time::ESPTime time) {
  this->add_no_result_to_queue_with_printf_("rtc0", "rtc0=%u", time.year);
  this->add_no_result_to_queue_with_printf_("rtc1", "rtc1=%u", time.month);
  this->add_no_result_to_queue_with_printf_("rtc2", "rtc2=%u", time.day_of_month);
  this->add_no_result_to_queue_with_printf_("rtc3", "rtc3=%u", time.hour);
  this->add_no_result_to_queue_with_printf_("rtc4", "rtc4=%u", time.minute);
  this->add_no_result_to_queue_with_printf_("rtc5", "rtc5=%u", time.second);
}
#endif

}  // namespace nextion
}  // namespace esphome
