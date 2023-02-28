#pragma once
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"
#include "ili9xxx_defines.h"
#include "ili9xxx_init.h"

namespace esphome {
namespace ili9xxx {

const uint32_t ILI9XXX_TRANSFER_BUFFER_SIZE = 64;

enum ILI9XXXColorMode {
  BITS_8 = 0x08,
  BITS_8_INDEXED = 0x09,
  BITS_16 = 0x10,
};

class ILI9XXXDisplay : public PollingComponent,
                       public display::DisplayBuffer,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_40MHZ> {
 public:
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }
  float get_setup_priority() const override;
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_palette(const uint8_t *palette) { this->palette_ = palette; }
  void set_buffer_color_mode(ILI9XXXColorMode color_mode) { this->buffer_color_mode_ = color_mode; }
  void set_dimentions(int16_t width, int16_t height) {
    this->height_ = height;
    this->width_ = width;
  }
  void command(uint8_t value);
  void data(uint8_t value);
  void send_command(uint8_t command_byte, const uint8_t *data_bytes, uint8_t num_data_bytes);
  uint8_t read_command(uint8_t command_byte, uint8_t index);

  void update() override;

  void fill(Color color) override;

  void dump_config() override;
  void setup() override;

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void setup_pins_();
  virtual void initialize() = 0;

  void display_();
  void init_lcd_(const uint8_t *init_cmd);
  void set_addr_window_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void invert_display_(bool invert);
  void reset_();

  int16_t width_{0};   ///< Display width as modified by current rotation
  int16_t height_{0};  ///< Display height as modified by current rotation
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

  uint16_t transfer_buffer_[ILI9XXX_TRANSFER_BUFFER_SIZE];

  uint32_t buffer_to_transfer_(uint32_t pos, uint32_t sz);

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *dc_pin_{nullptr};
  GPIOPin *busy_pin_{nullptr};

  bool prossing_update_ = false;
  bool need_update_ = false;
  bool is_18bitdisplay_ = false;
};

//-----------   M5Stack display --------------
class ILI9XXXM5Stack : public ILI9XXXDisplay {
 protected:
  void initialize() override;
};

//-----------   M5Stack display --------------
class ILI9XXXM5CORE : public ILI9XXXDisplay {
 protected:
  void initialize() override;
};

//-----------   ILI9XXX_24_TFT display --------------
class ILI9XXXILI9341 : public ILI9XXXDisplay {
 protected:
  void initialize() override;
};

//-----------   ILI9XXX_24_TFT rotated display --------------
class ILI9XXXILI9342 : public ILI9XXXDisplay {
 protected:
  void initialize() override;
};

//-----------   ILI9XXX_??_TFT rotated display --------------
class ILI9XXXILI9481 : public ILI9XXXDisplay {
 protected:
  void initialize() override;
};

//-----------   ILI9XXX_35_TFT rotated display --------------
class ILI9XXXILI9486 : public ILI9XXXDisplay {
 protected:
  void initialize() override;
};

//-----------   ILI9XXX_35_TFT rotated display --------------
class ILI9XXXILI9488 : public ILI9XXXDisplay {
 protected:
  void initialize() override;
};

//-----------   ILI9XXX_35_TFT rotated display --------------
class ILI9XXXST7796 : public ILI9XXXDisplay {
 protected:
  void initialize() override;
};

}  // namespace ili9xxx
}  // namespace esphome
