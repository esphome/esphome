#include "nextion.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace nextion {
static const char *const TAG = "nextion";

// Sleep safe commands
void Nextion::soft_reset() { this->send_command_("rest"); }

void Nextion::set_wake_up_page(uint8_t page_id) {
  this->add_no_result_to_queue_with_set_internal_("wake_up_page", "wup", page_id, true);
}

void Nextion::set_start_up_page(uint8_t page_id) { this->start_up_page_ = page_id; }

void Nextion::set_touch_sleep_timeout(uint16_t timeout) {
  if (timeout < 3) {
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

// Protocol reparse mode
bool Nextion::set_protocol_reparse_mode(bool active_mode) {
  ESP_LOGV(TAG, "Set Nextion protocol reparse mode: %s", YESNO(active_mode));
  this->ignore_is_setup_ = true;  // if not in reparse mode setup will fail, so it should be ignored
  bool all_commands_sent = true;
  if (active_mode) {  // Sets active protocol reparse mode
    all_commands_sent &= this->send_command_("recmod=1");
  } else {  // Sets passive protocol reparse mode
    all_commands_sent &=
        this->send_command_("DRAKJHSUYDGBNCJHGJKSHBDN");   // To exit active reparse mode this sequence must be sent
    all_commands_sent &= this->send_command_("recmod=0");  // Sending recmode=0 twice is recommended
    all_commands_sent &= this->send_command_("recmod=0");
  }
  if (!this->nextion_reports_is_setup_) {  // No need to connect if is already setup
    all_commands_sent &= this->send_command_("connect");
  }
  this->ignore_is_setup_ = false;
  return all_commands_sent;
}
void Nextion::set_exit_reparse_on_start(bool exit_reparse) { this->exit_reparse_on_start_ = exit_reparse; }

// Set Colors - Background
void Nextion::set_component_background_color(const char *component, uint16_t color) {
  this->add_no_result_to_queue_with_printf_("set_component_background_color", "%s.bco=%" PRIu16, component, color);
}

void Nextion::set_component_background_color(const char *component, const char *color) {
  this->add_no_result_to_queue_with_printf_("set_component_background_color", "%s.bco=%s", component, color);
}

void Nextion::set_component_background_color(const char *component, Color color) {
  this->add_no_result_to_queue_with_printf_("set_component_background_color", "%s.bco=%d", component,
                                            display::ColorUtil::color_to_565(color));
}

// Set Colors - Background (pressed)
void Nextion::set_component_pressed_background_color(const char *component, uint16_t color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_background_color", "%s.bco2=%" PRIu16, component,
                                            color);
}

void Nextion::set_component_pressed_background_color(const char *component, const char *color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_background_color", "%s.bco2=%s", component, color);
}

void Nextion::set_component_pressed_background_color(const char *component, Color color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_background_color", "%s.bco2=%d", component,
                                            display::ColorUtil::color_to_565(color));
}

// Set Colors - Foreground
void Nextion::set_component_foreground_color(const char *component, uint16_t color) {
  this->add_no_result_to_queue_with_printf_("set_component_foreground_color", "%s.pco=%" PRIu16, component, color);
}

void Nextion::set_component_foreground_color(const char *component, const char *color) {
  this->add_no_result_to_queue_with_printf_("set_component_foreground_color", "%s.pco=%s", component, color);
}

void Nextion::set_component_foreground_color(const char *component, Color color) {
  this->add_no_result_to_queue_with_printf_("set_component_foreground_color", "%s.pco=%d", component,
                                            display::ColorUtil::color_to_565(color));
}

// Set Colors - Foreground (pressed)
void Nextion::set_component_pressed_foreground_color(const char *component, uint16_t color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_foreground_color", "%s.pco2=%" PRIu16, component,
                                            color);
}

void Nextion::set_component_pressed_foreground_color(const char *component, const char *color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_foreground_color", " %s.pco2=%s", component, color);
}

void Nextion::set_component_pressed_foreground_color(const char *component, Color color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_foreground_color", "%s.pco2=%d", component,
                                            display::ColorUtil::color_to_565(color));
}

// Set Colors - Font
void Nextion::set_component_font_color(const char *component, uint16_t color) {
  this->add_no_result_to_queue_with_printf_("set_component_font_color", "%s.pco=%" PRIu16, component, color);
}

void Nextion::set_component_font_color(const char *component, const char *color) {
  this->add_no_result_to_queue_with_printf_("set_component_font_color", "%s.pco=%s", component, color);
}

void Nextion::set_component_font_color(const char *component, Color color) {
  this->add_no_result_to_queue_with_printf_("set_component_font_color", "%s.pco=%d", component,
                                            display::ColorUtil::color_to_565(color));
}

// Set Colors - Font (pressed)
void Nextion::set_component_pressed_font_color(const char *component, uint16_t color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_font_color", "%s.pco2=%" PRIu16, component, color);
}

void Nextion::set_component_pressed_font_color(const char *component, const char *color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_font_color", " %s.pco2=%s", component, color);
}

void Nextion::set_component_pressed_font_color(const char *component, Color color) {
  this->add_no_result_to_queue_with_printf_("set_component_pressed_font_color", "%s.pco2=%d", component,
                                            display::ColorUtil::color_to_565(color));
}

// Set picture
void Nextion::set_component_pic(const char *component, uint8_t pic_id) {
  this->add_no_result_to_queue_with_printf_("set_component_pic", "%s.pic=%" PRIu8, component, pic_id);
}

void Nextion::set_component_picc(const char *component, uint8_t pic_id) {
  this->add_no_result_to_queue_with_printf_("set_component_pic", "%s.picc=%" PRIu8, component, pic_id);
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
void Nextion::goto_page(uint8_t page) { this->add_no_result_to_queue_with_printf_("goto_page", "page %i", page); }

void Nextion::set_backlight_brightness(float brightness) {
  if (brightness < 0 || brightness > 1.0) {
    ESP_LOGD(TAG, "Brightness out of bounds, percentage range 0-1.0");
    return;
  }
  this->add_no_result_to_queue_with_printf_("backlight_brightness", "dim=%d", static_cast<int>(brightness * 100));
}

void Nextion::set_auto_wake_on_touch(bool auto_wake) {
  this->add_no_result_to_queue_with_set("auto_wake_on_touch", "thup", auto_wake ? 1 : 0);
}

// General Component
void Nextion::set_component_font(const char *component, uint8_t font_id) {
  this->add_no_result_to_queue_with_printf_("set_component_font", "%s.font=%" PRIu8, component, font_id);
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

void Nextion::set_component_picture(const char *component, uint8_t picture_id) {
  this->add_no_result_to_queue_with_printf_("set_component_picture", "%s.pic=%" PRIu8, component, picture_id);
}

void Nextion::set_component_text(const char *component, const char *text) {
  this->add_no_result_to_queue_with_printf_("set_component_text", "%s.txt=\"%s\"", component, text);
}

void Nextion::set_component_value(const char *component, int32_t value) {
  this->add_no_result_to_queue_with_printf_("set_component_value", "%s.val=%" PRId32, component, value);
}

void Nextion::add_waveform_data(uint8_t component_id, uint8_t channel_number, uint8_t value) {
  this->add_no_result_to_queue_with_printf_("add_waveform_data", "add %" PRIu8 ",%" PRIu8 ",%" PRIu8, component_id,
                                            channel_number, value);
}

void Nextion::open_waveform_channel(uint8_t component_id, uint8_t channel_number, uint8_t value) {
  this->add_no_result_to_queue_with_printf_("open_waveform_channel", "addt %" PRIu8 ",%" PRIu8 ",%" PRIu8, component_id,
                                            channel_number, value);
}

void Nextion::set_component_coordinates(const char *component, uint16_t x, uint16_t y) {
  this->add_no_result_to_queue_with_printf_("set_component_coordinates command 1", "%s.xcen=%" PRIu16, component, x);
  this->add_no_result_to_queue_with_printf_("set_component_coordinates command 2", "%s.ycen=%" PRIu16, component, y);
}

// Drawing
void Nextion::display_picture(uint16_t picture_id, uint16_t x_start, uint16_t y_start) {
  this->add_no_result_to_queue_with_printf_("display_picture", "pic %" PRIu16 ", %" PRIu16 ", %" PRIu16, x_start,
                                            y_start, picture_id);
}

void Nextion::fill_area(uint16_t x1, uint16_t y1, uint16_t width, uint16_t height, uint16_t color) {
  this->add_no_result_to_queue_with_printf_(
      "fill_area", "fill %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16, x1, y1, width, height, color);
}

void Nextion::fill_area(uint16_t x1, uint16_t y1, uint16_t width, uint16_t height, const char *color) {
  this->add_no_result_to_queue_with_printf_("fill_area", "fill %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%s", x1,
                                            y1, width, height, color);
}

void Nextion::fill_area(uint16_t x1, uint16_t y1, uint16_t width, uint16_t height, Color color) {
  this->add_no_result_to_queue_with_printf_("fill_area",
                                            "fill %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16, x1, y1,
                                            width, height, display::ColorUtil::color_to_565(color));
}

void Nextion::line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
  this->add_no_result_to_queue_with_printf_("line", "line %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16, x1,
                                            y1, x2, y2, color);
}

void Nextion::line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const char *color) {
  this->add_no_result_to_queue_with_printf_("line", "line %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%s", x1, y1,
                                            x2, y2, color);
}

void Nextion::line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, Color color) {
  this->add_no_result_to_queue_with_printf_("line", "line %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16, x1,
                                            y1, x2, y2, display::ColorUtil::color_to_565(color));
}

void Nextion::rectangle(uint16_t x1, uint16_t y1, uint16_t width, uint16_t height, uint16_t color) {
  this->add_no_result_to_queue_with_printf_("draw", "draw %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16, x1,
                                            y1, static_cast<uint16_t>(x1 + width), static_cast<uint16_t>(y1 + height),
                                            color);
}

void Nextion::rectangle(uint16_t x1, uint16_t y1, uint16_t width, uint16_t height, const char *color) {
  this->add_no_result_to_queue_with_printf_("draw", "draw %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%s", x1, y1,
                                            static_cast<uint16_t>(x1 + width), static_cast<uint16_t>(y1 + height),
                                            color);
}

void Nextion::rectangle(uint16_t x1, uint16_t y1, uint16_t width, uint16_t height, Color color) {
  this->add_no_result_to_queue_with_printf_("draw", "draw %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16, x1,
                                            y1, static_cast<uint16_t>(x1 + width), static_cast<uint16_t>(y1 + height),
                                            display::ColorUtil::color_to_565(color));
}

void Nextion::circle(uint16_t center_x, uint16_t center_y, uint16_t radius, uint16_t color) {
  this->add_no_result_to_queue_with_printf_("cir", "cir %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16, center_x,
                                            center_y, radius, color);
}

void Nextion::circle(uint16_t center_x, uint16_t center_y, uint16_t radius, const char *color) {
  this->add_no_result_to_queue_with_printf_("cir", "cir %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%s", center_x, center_y,
                                            radius, color);
}

void Nextion::circle(uint16_t center_x, uint16_t center_y, uint16_t radius, Color color) {
  this->add_no_result_to_queue_with_printf_("cir", "cir %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16, center_x,
                                            center_y, radius, display::ColorUtil::color_to_565(color));
}

void Nextion::filled_circle(uint16_t center_x, uint16_t center_y, uint16_t radius, uint16_t color) {
  this->add_no_result_to_queue_with_printf_("cirs", "cirs %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16, center_x,
                                            center_y, radius, color);
}

void Nextion::filled_circle(uint16_t center_x, uint16_t center_y, uint16_t radius, const char *color) {
  this->add_no_result_to_queue_with_printf_("cirs", "cirs %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%s", center_x, center_y,
                                            radius, color);
}

void Nextion::filled_circle(uint16_t center_x, uint16_t center_y, uint16_t radius, Color color) {
  this->add_no_result_to_queue_with_printf_("cirs", "cirs %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16, center_x,
                                            center_y, radius, display::ColorUtil::color_to_565(color));
}

void Nextion::qrcode(uint16_t x1, uint16_t y1, const char *content, uint16_t size, uint16_t background_color,
                     uint16_t foreground_color, uint8_t logo_pic, uint8_t border_width) {
  this->add_no_result_to_queue_with_printf_(
      "qrcode", "qrcode %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu8 ",%" PRIu8 ",\"%s\"", x1,
      y1, size, background_color, foreground_color, logo_pic, border_width, content);
}

void Nextion::qrcode(uint16_t x1, uint16_t y1, const char *content, uint16_t size, Color background_color,
                     Color foreground_color, uint8_t logo_pic, uint8_t border_width) {
  this->add_no_result_to_queue_with_printf_(
      "qrcode", "qrcode %" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu16 ",%" PRIu8 ",%" PRIu8 ",\"%s\"", x1,
      y1, size, display::ColorUtil::color_to_565(background_color), display::ColorUtil::color_to_565(foreground_color),
      logo_pic, border_width, content);
}

void Nextion::set_nextion_rtc_time(ESPTime time) {
  this->add_no_result_to_queue_with_printf_("rtc0", "rtc0=%u", time.year);
  this->add_no_result_to_queue_with_printf_("rtc1", "rtc1=%u", time.month);
  this->add_no_result_to_queue_with_printf_("rtc2", "rtc2=%u", time.day_of_month);
  this->add_no_result_to_queue_with_printf_("rtc3", "rtc3=%u", time.hour);
  this->add_no_result_to_queue_with_printf_("rtc4", "rtc4=%u", time.minute);
  this->add_no_result_to_queue_with_printf_("rtc5", "rtc5=%u", time.second);
}

}  // namespace nextion
}  // namespace esphome
