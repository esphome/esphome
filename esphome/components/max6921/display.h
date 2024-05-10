#pragma once

#include <esp32-hal-gpio.h>
#include <string>
#include <vector>
#include "esphome/core/time.h"

namespace esphome {
namespace max6921 {

class MAX6921Component;

#define FONT_SIZE           95
#define DISPLAY_TEXT_LEN    FONT_SIZE   // at least font size for demo mode "scroll font"

enum display_mode_t {
  DISP_MODE_PRINT,          // input by it-functions
  DISP_MODE_OTHER,          // input by actions
  DISP_MODE_LAST_ENUM
};

enum display_scroll_mode_t {
  DISP_SCROLL_MODE_OFF,     // show text at given position, cut if too long
  DISP_SCROLL_MODE_LEFT,    // scroll left, start with 1st char at right position
  DISP_SCROLL_MODE_LAST_ENUM
};

enum demo_mode_t {
  DEMO_MODE_OFF,
  DEMO_MODE_SCROLL_FONT,
};

class DisplayBrightness
{
 public:
  uint32_t config_brightness_pwm(uint8_t pwm_pin_no, uint8_t channel,
                                 uint8_t resolution, uint32_t freq_wanted);
  float get_brightness(void) { return this->brightness_cfg_value_; }
  void set_brightness(float percent);

 protected:
  float brightness_cfg_value_;        // brightness in percent (0.0-1.0)
  bool brightness_cfg_changed_;
  uint32_t brightness_max_duty_;
  uint8_t brightness_pwm_channel_;
};

class DisplayText
{
 public:
  bool content_changed;
  uint max_pos;                       // max. display position
  uint start_pos;                     // current display start position (0..n)
  char text[DISPLAY_TEXT_LEN + 1];    // current text to display (may be larger then display)
  uint visible_idx;                   // current index of start of visible part
  uint visible_len;                   // current length of visible text
  DisplayText();
  int set(uint start_pos, uint max_pos, const char *str);
};

class DisplayMode
{
 public:
  display_mode_t mode;                                    // display mode
  DisplayMode();
  void set_mode(display_mode_t display_mode);

 protected:

};

class DisplayScrollMode
{
 public:
  display_scroll_mode_t scroll_mode;                      // scroll mode
  uint8_t cycle_num;
  DisplayScrollMode();
  void set_scroll_mode(DisplayText *disp_text, display_scroll_mode_t scroll_mode, uint8_t cycle_num=0);

 protected:
  DisplayText *disp_text_;
  void init_scroll_mode_(void);
  void scroll_left_(DisplayText& disp_text);
};

class Display : public DisplayBrightness,
                public DisplayMode,
                public DisplayScrollMode
{
 public:
  Display(MAX6921Component *max6921) { max6921_ = max6921; }
  void clear(int pos=-1);
  void dump_config();
  bool isPointSegOnly(char c);
  void setup(std::vector<uint8_t>& seg_to_out_map, std::vector<uint8_t>& pos_to_out_map);
  void set_demo_mode(demo_mode_t mode, uint8_t cycle_num);
  void set_demo_mode(const std::string& mode, uint8_t cycle_num);
  int set_text(uint8_t start_pos, const char *str);
  void update(void);

 protected:
  MAX6921Component *max6921_;
  std::vector<uint8_t> seg_to_out_map_;                    // mapping of display segments to MAX6921 OUT pins
  std::vector<uint8_t> pos_to_out_map_;                    // mapping of display positions to MAX6921 OUT pins
  uint num_digits_;                                        // number of display positions
  uint8_t *ascii_out_data_;
  uint8_t *out_buf_;                                       // current MAX9621 data (3 bytes for every display position)
  size_t out_buf_size_;
  uint seg_out_smallest_;
  uint32_t refresh_period_us_;
  DisplayText disp_text_;
  static void display_refresh_task_(void *pv);
  int update_out_buf_(DisplayText& disp_text);

 private:
  void init_font__(void);
};


}  // namespace max6921
}  // namespace esphome
