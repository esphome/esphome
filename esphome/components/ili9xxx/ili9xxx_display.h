#pragma once
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"
#include "ili9xxx_defines.h"
#include "ili9xxx_init.h"

namespace esphome {
namespace ili9xxx {

const size_t ILI9XXX_TRANSFER_BUFFER_SIZE = 126;  // ensure this is divisible by 6

enum ILI9XXXColorMode {
  BITS_8 = 0x08,
  BITS_8_INDEXED = 0x09,
  BITS_16 = 0x10,
};

#ifndef ILI9XXXDisplay_DATA_RATE
#define ILI9XXXDisplay_DATA_RATE spi::DATA_RATE_40MHZ
#endif  // ILI9XXXDisplay_DATA_RATE

class ILI9XXXDisplay : public display::DisplayBuffer,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_LEADING, ILI9XXXDisplay_DATA_RATE> {
 public:
  ILI9XXXDisplay() = default;
  ILI9XXXDisplay(uint8_t const *init_sequence, int16_t width, int16_t height, bool invert_colors)
      : init_sequence_{init_sequence}, width_{width}, height_{height}, pre_invertcolors_{invert_colors} {
    uint8_t cmd, num_args, bits;
    const uint8_t *addr = init_sequence;
    while ((cmd = *addr++) != 0) {
      num_args = *addr++ & 0x7F;
      if (cmd == ILI9XXX_MADCTL) {
        bits = *addr;
        this->swap_xy_ = (bits & MADCTL_MV) != 0;
        this->mirror_x_ = (bits & MADCTL_MX) != 0;
        this->mirror_y_ = (bits & MADCTL_MY) != 0;
        this->color_order_ = (bits & MADCTL_BGR) ? display::COLOR_ORDER_BGR : display::COLOR_ORDER_RGB;
        break;
      }
      addr += num_args;
    }
  }

  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }
  float get_setup_priority() const override;
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_palette(const uint8_t *palette) { this->palette_ = palette; }
  void set_buffer_color_mode(ILI9XXXColorMode color_mode) { this->buffer_color_mode_ = color_mode; }
  void set_dimensions(int16_t width, int16_t height) {
    this->height_ = height;
    this->width_ = width;
  }
  void set_offsets(int16_t offset_x, int16_t offset_y) {
    this->offset_x_ = offset_x;
    this->offset_y_ = offset_y;
  }
  void invert_colors(bool invert);
  void command(uint8_t value);
  void data(uint8_t value);
  void send_command(uint8_t command_byte, const uint8_t *data_bytes, uint8_t num_data_bytes);
  uint8_t read_command(uint8_t command_byte, uint8_t index);
  void set_color_order(display::ColorOrder color_order) { this->color_order_ = color_order; }
  void set_swap_xy(bool swap_xy) { this->swap_xy_ = swap_xy; }
  void set_mirror_x(bool mirror_x) { this->mirror_x_ = mirror_x; }
  void set_mirror_y(bool mirror_y) { this->mirror_y_ = mirror_y; }

  void update() override;

  void fill(Color color) override;

  void dump_config() override;
  void setup() override;

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void setup_pins_();

  void display_();
  void init_lcd_();
  void set_addr_window_(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2);
  void reset_();

  uint8_t const *init_sequence_{};
  int16_t width_{0};   ///< Display width as modified by current rotation
  int16_t height_{0};  ///< Display height as modified by current rotation
  int16_t offset_x_{0};
  int16_t offset_y_{0};
  uint16_t x_low_{0};
  uint16_t y_low_{0};
  uint16_t x_high_{0};
  uint16_t y_high_{0};
  const uint8_t *palette_;

  ILI9XXXColorMode buffer_color_mode_{BITS_16};

  uint32_t get_buffer_length_();
  int get_width_internal() override;
  int get_height_internal() override;

  void start_command_();
  void end_command_();
  void start_data_();
  void end_data_();

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *dc_pin_{nullptr};
  GPIOPin *busy_pin_{nullptr};

  bool prossing_update_ = false;
  bool need_update_ = false;
  bool is_18bitdisplay_ = false;
  bool pre_invertcolors_ = false;
  display::ColorOrder color_order_{};
  bool swap_xy_{};
  bool mirror_x_{};
  bool mirror_y_{};
};

//-----------   M5Stack display --------------
class ILI9XXXM5Stack : public ILI9XXXDisplay {
 public:
  ILI9XXXM5Stack() : ILI9XXXDisplay(INITCMD_M5STACK, 320, 240, true) {}
};

//-----------   M5Stack display --------------
class ILI9XXXM5CORE : public ILI9XXXDisplay {
 public:
  ILI9XXXM5CORE() : ILI9XXXDisplay(INITCMD_M5CORE, 320, 240, true) {}
};

//-----------   ST7789V display --------------
class ILI9XXXST7789V : public ILI9XXXDisplay {
 public:
  ILI9XXXST7789V() : ILI9XXXDisplay(INITCMD_ST7789V, 240, 320, false) {}
};

//-----------   ILI9XXX_24_TFT display --------------
class ILI9XXXILI9341 : public ILI9XXXDisplay {
 public:
  ILI9XXXILI9341() : ILI9XXXDisplay(INITCMD_ILI9341, 240, 320, false) {}
};

//-----------   ILI9XXX_24_TFT rotated display --------------
class ILI9XXXILI9342 : public ILI9XXXDisplay {
 public:
  ILI9XXXILI9342() : ILI9XXXDisplay(INITCMD_ILI9341, 320, 240, false) {}
};

//-----------   ILI9XXX_??_TFT rotated display --------------
class ILI9XXXILI9481 : public ILI9XXXDisplay {
 public:
  ILI9XXXILI9481() : ILI9XXXDisplay(INITCMD_ILI9481, 480, 320, false) {}
};

//-----------   ILI9481 in 18 bit mode --------------
class ILI9XXXILI948118 : public ILI9XXXDisplay {
 public:
  ILI9XXXILI948118() : ILI9XXXDisplay(INITCMD_ILI9481_18, 320, 480, true) {}
};

//-----------   ILI9XXX_35_TFT rotated display --------------
class ILI9XXXILI9486 : public ILI9XXXDisplay {
 public:
  ILI9XXXILI9486() : ILI9XXXDisplay(INITCMD_ILI9486, 480, 320, false) {}
};

//-----------   ILI9XXX_35_TFT rotated display --------------
class ILI9XXXILI9488 : public ILI9XXXDisplay {
 public:
  ILI9XXXILI9488() : ILI9XXXDisplay(INITCMD_ILI9488, 480, 320, true) {}
};

//-----------   ILI9XXX_35_TFT origin colors rotated display --------------
class ILI9XXXILI9488A : public ILI9XXXDisplay {
 public:
  ILI9XXXILI9488A() : ILI9XXXDisplay(INITCMD_ILI9488_A, 480, 320, true) {}
};

//-----------   ILI9XXX_35_TFT rotated display --------------
class ILI9XXXST7796 : public ILI9XXXDisplay {
 public:
  ILI9XXXST7796() : ILI9XXXDisplay(INITCMD_ST7796, 320, 480, false) {}
};

class ILI9XXXS3Box : public ILI9XXXDisplay {
 public:
  ILI9XXXS3Box() : ILI9XXXDisplay(INITCMD_S3BOX, 320, 240, false) {}
};

class ILI9XXXS3BoxLite : public ILI9XXXDisplay {
 public:
  ILI9XXXS3BoxLite() : ILI9XXXDisplay(INITCMD_S3BOXLITE, 320, 240, true) {}
};

}  // namespace ili9xxx
}  // namespace esphome
