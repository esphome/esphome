#pragma once

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include <string>
#include <vector>
#include "esphome/core/time.h"

namespace esphome {
namespace max6921 {

class MAX6921Component;

static const uint FONT_SIZE = 95;
static const uint DISPLAY_TEXT_LEN = FONT_SIZE;  // at least font size for demo mode "scroll font"

enum DisplayModeT {
  // Used as index!
  DISP_MODE_PRINT = 0,  // input by it-functions
  DISP_MODE_OTHER,      // input by actions
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

class Max6921DisplayMode {
 public:
  DisplayModeT mode;
  Max6921DisplayMode();
  DisplayModeT set_mode(DisplayModeT mode);

 protected:
};

class Max6921DisplayText {
 public:
  bool content_changed;
  bool repeat_on;                   // repetitions are enabled
  uint max_pos;                     // max. display position
  uint start_pos;                   // current display start position (0..n)
  char text[DISPLAY_TEXT_LEN + 1];  // current text to display (may be larger then display)
  uint visible_idx;                 // current index of start of visible part
  uint visible_len;                 // current length of visible text
  TextAlignT align;
  TextEffectT effect;
  uint32_t duration_ms;         // text/effect duration
  uint32_t repeat_interval_ms;  // repeat interval
  uint8_t repeat_num;           // number of repeats of showing text/effect
  uint8_t repeat_current;       // current repeat cycle
  uint8_t cycle_num;            // number of effect cycles
  uint8_t cycle_current;        // current effect cycle
  uint32_t update_interval_ms;  // effect update interval
  Max6921DisplayText();
  bool blink();
  uint32_t get_duration_start() { return this->duration_ms_start_; }
  uint32_t get_repeat_start() { return this->repeat_ms_start_; }
  bool scroll_left();
  void set_duration(uint32_t duration_ms);
  void start_duration();
  void start_repeat();
  void set_repeats(uint8_t repeat_num, uint32_t repeat_interval_ms);
  int set_text(uint start_pos, uint max_pos, const std::string &text);
  void set_text_align(TextAlignT align);
  void set_text_align(const std::string &align);
  void set_text_effect(TextEffectT effect, uint8_t cycle_num = 0, uint32_t update_interval = 0);
  void set_text_effect(const std::string &effect, uint8_t cycle_num = 0, uint32_t update_interval = 0);

 protected:
  uint32_t duration_ms_start_;
  uint32_t repeat_ms_start_;
  void init_text_align_();
  void init_text_effect_();
};

class Max6921Display : public Max6921DisplayMode {
 public:
  Max6921Display(MAX6921Component *max6921) { max6921_ = max6921; }
  void clear(int pos = -1);
  void dump_config();
  bool is_point_seg_only(char c);
  void restore_update_interval();
  void setup(std::vector<uint8_t> &seg_to_out_map, std::vector<uint8_t> &pos_to_out_map);
  void set_demo_mode(DemoModeT mode, uint32_t interval, uint8_t cycle_num);
  void set_demo_mode(const std::string &mode, uint32_t interval, uint8_t cycle_num);
  DisplayModeT set_mode(DisplayModeT mode);
  int set_text(const char *text, uint8_t start_pos);
  int set_text(const std::string &text, uint8_t start_pos, const std::string &align, uint32_t duration,
               const std::string &effect, uint8_t effect_cycle_num, uint8_t repeat_num, uint32_t repeat_interval,
               uint32_t update_interval);
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
  Max6921DisplayText disp_text_ctrl_[DISP_MODE_LAST_ENUM];
  Max6921DisplayText &disp_text_ = disp_text_ctrl_[0];
  uint32_t default_update_interval_;
  static void display_refresh_task(void *pv);
  void init_font_();
  int update_out_buf_();
};

}  // namespace max6921
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
