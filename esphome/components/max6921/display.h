#pragma once

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include <esp32-hal-gpio.h>
#include <string>
#include <vector>
#include "esphome/core/time.h"

namespace esphome {
namespace max6921 {

class MAX6921Component;

static const uint FONT_SIZE = 95;
static const uint DISPLAY_TEXT_LEN = FONT_SIZE;  // at least font size for demo mode "scroll font"

enum DisplayModeT {
  DISP_MODE_PRINT,  // input by it-functions
  DISP_MODE_OTHER,  // input by actions
  DISP_MODE_LAST_ENUM
};

enum TextAlignT { TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER, TEXT_ALIGN_RIGHT, TEXT_ALIGN_LAST_ENUM };

enum TextEffectT {
  TEXT_EFFECT_NONE,         // show text at given position, cut if too long
  TEXT_EFFECT_BLINK,        // blink
  TEXT_EFFECT_SCROLL_LEFT,  // scroll left, start with 1st char at right position
  // TEXT_EFFECT_SCROLL_RIGHT, // scroll right, start with last char at left position
  TEXT_EFFECT_LAST_ENUM
};

enum DemoModeT {
  DEMO_MODE_OFF,
  DEMO_MODE_SCROLL_FONT,
};

class DisplayBrightness {
 public:
  uint32_t config_brightness_pwm(uint8_t pwm_pin_no, uint8_t channel, uint8_t resolution, uint32_t freq_wanted);
  float get_brightness() { return this->brightness_cfg_value_; }
  void set_brightness(float percent);

 protected:
  float brightness_cfg_value_;  // brightness in percent (0.0-1.0)
  bool brightness_cfg_changed_;
  uint32_t brightness_max_duty_;
  uint8_t brightness_pwm_channel_;
};

class DisplayMode {
 public:
  DisplayModeT mode;
  uint32_t duration_ms;
  DisplayMode();
  void set_mode(DisplayModeT mode, uint32_t duration_ms = 0);

 protected:
  uint32_t duration_ms_start_;
};

class DisplayText {
 public:
  bool content_changed;
  uint max_pos;                     // max. display position
  uint start_pos;                   // current display start position (0..n)
  char text[DISPLAY_TEXT_LEN + 1];  // current text to display (may be larger then display)
  uint visible_idx;                 // current index of start of visible part
  uint visible_len;                 // current length of visible text
  TextAlignT align;
  TextEffectT effect;
  uint8_t cycle_num;
  DisplayText();
  void blink();
  void scroll_left();
  int set_text(uint start_pos, uint max_pos, const std::string &text);
  void set_text_align(TextAlignT align);
  void set_text_align(const std::string &align);
  void set_text_effect(TextEffectT effect, uint8_t cycle_num = 0);
  void set_text_effect(const std::string &effect, uint8_t cycle_num = 0);

 protected:
  int effect_change_count_;
  void init_text_align_();
  void init_text_effect_();
};

class Display : public DisplayBrightness, public DisplayMode {
 public:
  Display(MAX6921Component *max6921) { max6921_ = max6921; }
  void clear(int pos = -1);
  void dump_config();
  bool is_point_seg_only(char c);
  void restore_update_interval();
  void setup(std::vector<uint8_t> &seg_to_out_map, std::vector<uint8_t> &pos_to_out_map);
  void set_demo_mode(DemoModeT mode, uint32_t interval, uint8_t cycle_num);
  void set_demo_mode(const std::string &mode, uint32_t interval, uint8_t cycle_num);
  int set_text(const char *text, uint8_t start_pos);
  int set_text(const std::string &text, uint8_t start_pos, const std::string &align, uint32_t duration,
               const std::string &effect, uint32_t interval, uint8_t cycle_num);
  void set_update_interval(uint32_t interval_ms);
  void update();

 protected:
  MAX6921Component *max6921_;
  std::vector<uint8_t> seg_to_out_map_;  // mapping of display segments to MAX6921 OUT pins
  std::vector<uint8_t> pos_to_out_map_;  // mapping of display positions to MAX6921 OUT pins
  uint num_digits_;                      // number of display positions
  uint8_t *ascii_out_data_;
  uint8_t *out_buf_;  // current MAX9621 data (3 bytes for every display position)
  size_t out_buf_size_;
  uint seg_out_smallest_;
  uint32_t refresh_period_us_;
  DisplayText disp_text_;
  uint32_t default_update_interval_;
  static void display_refresh_task(void *pv);
  int update_out_buf_();

 private:
  void init_font__();
};

}  // namespace max6921
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
