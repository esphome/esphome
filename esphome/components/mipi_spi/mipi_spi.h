#pragma once

#include <utility>

#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display.h"
#include "esphome/components/display/display_color_utils.h"

namespace esphome {
namespace mipi_spi {

constexpr static const char *const TAG = "display.mipi_spi";
static constexpr uint8_t SW_RESET_CMD = 0x01;
static constexpr uint8_t SLEEP_OUT = 0x11;
static constexpr uint8_t NORON = 0x13;
static constexpr uint8_t INVERT_OFF = 0x20;
static constexpr uint8_t INVERT_ON = 0x21;
static constexpr uint8_t ALL_ON = 0x23;
static constexpr uint8_t WRAM = 0x24;
static constexpr uint8_t MIPI = 0x26;
static constexpr uint8_t DISPLAY_ON = 0x29;
static constexpr uint8_t RASET = 0x2B;
static constexpr uint8_t CASET = 0x2A;
static constexpr uint8_t WDATA = 0x2C;
static constexpr uint8_t TEON = 0x35;
static constexpr uint8_t MADCTL_CMD = 0x36;
static constexpr uint8_t PIXFMT = 0x3A;
static constexpr uint8_t BRIGHTNESS = 0x51;
static constexpr uint8_t SWIRE1 = 0x5A;
static constexpr uint8_t SWIRE2 = 0x5B;
static constexpr uint8_t PAGESEL = 0xFE;

static constexpr uint8_t MADCTL_MY = 0x80;     // Bit 7 Bottom to top
static constexpr uint8_t MADCTL_MX = 0x40;     // Bit 6 Right to left
static constexpr uint8_t MADCTL_MV = 0x20;     // Bit 5 Swap axes
static constexpr uint8_t MADCTL_RGB = 0x00;    // Bit 3 Red-Green-Blue pixel order
static constexpr uint8_t MADCTL_BGR = 0x08;    // Bit 3 Blue-Green-Red pixel order
static constexpr uint8_t MADCTL_XFLIP = 0x02;  // Mirror the display horizontally
static constexpr uint8_t MADCTL_YFLIP = 0x01;  // Mirror the display vertically

static constexpr uint8_t DELAY_FLAG = 0xFF;
// store a 16 bit value in a buffer, big endian.
static inline void put16_be(uint8_t *buf, uint16_t value) {
  buf[0] = value >> 8;
  buf[1] = value;
}

// Buffer mode, conveniently also the number of bytes in a pixel
enum PixelMode {
  PIXEL_MODE_8 = 1,
  PIXEL_MODE_16 = 2,
  PIXEL_MODE_18 = 3,
};

enum BusType {
  BUS_TYPE_SINGLE = 1,
  BUS_TYPE_QUAD = 4,
  BUS_TYPE_OCTAL = 8,
  BUS_TYPE_SINGLE_16 = 16,  // Single bit bus, but 16 bits per transfer
};

/**
 * Base class for MIPI SPI displays.
 * All the methods are defined here in the header file, as it is not possible to define templated methods in a cpp file.
 *
 * @tparam BUFFERTYPE The type of the buffer pixels, e.g. uint8_t or uint16_t
 * @tparam BUFFERPIXEL Color depth of the buffer
 * @tparam DISPLAYPIXEL Color depth of the display
 * @tparam BUS_TYPE The type of the interface bus (single, quad, octal)
 * @tparam WIDTH Width of the display in pixels
 * @tparam HEIGHT Height of the display in pixels
 * @tparam OFFSET_WIDTH The x-offset of the display in pixels
 * @tparam OFFSET_HEIGHT The y-offset of the display in pixels
 * buffer
 */
template<typename BUFFERTYPE, PixelMode BUFFERPIXEL, bool IS_BIG_ENDIAN, PixelMode DISPLAYPIXEL, BusType BUS_TYPE,
         int WIDTH, int HEIGHT, int OFFSET_WIDTH, int OFFSET_HEIGHT>
class MipiSpi : public display::Display,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_1MHZ> {
 public:
  MipiSpi() = default;
  void update() override { this->stop_poller(); }
  void draw_pixel_at(int x, int y, Color color) override {}
  void set_model(const char *model) { this->model_ = model; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_enable_pins(std::vector<GPIOPin *> enable_pins) { this->enable_pins_ = std::move(enable_pins); }
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_invert_colors(bool invert_colors) {
    this->invert_colors_ = invert_colors;
    this->reset_params_();
  }
  void set_brightness(uint8_t brightness) {
    this->brightness_ = brightness;
    this->reset_params_();
  }
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

  int get_width_internal() override { return WIDTH; }
  int get_height_internal() override { return HEIGHT; }
  void set_init_sequence(const std::vector<uint8_t> &sequence) { this->init_sequence_ = sequence; }

  // reset the display, and write the init sequence
  void setup() override {
    this->spi_setup();
    if (this->dc_pin_ != nullptr) {
      this->dc_pin_->setup();
      this->dc_pin_->digital_write(false);
    }
    for (auto *pin : this->enable_pins_) {
      pin->setup();
      pin->digital_write(true);
    }
    if (this->reset_pin_ != nullptr) {
      this->reset_pin_->setup();
      this->reset_pin_->digital_write(true);
      delay(5);
      this->reset_pin_->digital_write(false);
      delay(5);
      this->reset_pin_->digital_write(true);
    }

    // need to know when the display is ready for SLPOUT command - will be 120ms after reset
    auto when = millis() + 120;
    delay(10);
    size_t index = 0;
    auto &vec = this->init_sequence_;
    while (index != vec.size()) {
      if (vec.size() - index < 2) {
        esph_log_e(TAG, "Malformed init sequence");
        this->mark_failed();
        return;
      }
      uint8_t cmd = vec[index++];
      uint8_t x = vec[index++];
      if (x == DELAY_FLAG) {
        esph_log_d(TAG, "Delay %dms", cmd);
        delay(cmd);
      } else {
        uint8_t num_args = x & 0x7F;
        if (vec.size() - index < num_args) {
          esph_log_e(TAG, "Malformed init sequence");
          this->mark_failed();
          return;
        }
        auto arg_byte = vec[index];
        switch (cmd) {
          case SLEEP_OUT: {
            // are we ready, boots?
            int duration = when - millis();
            if (duration > 0) {
              esph_log_d(TAG, "Sleep %dms", duration);
              delay(duration);
            }
          } break;

          case INVERT_ON:
            this->invert_colors_ = true;
            break;
          case MADCTL_CMD:
            this->madctl_ = arg_byte;
            break;
          case BRIGHTNESS:
            this->brightness_ = arg_byte;
            break;

          default:
            break;
        }
        const auto *ptr = vec.data() + index;
        esph_log_d(TAG, "Command %02X, length %d, byte %02X", cmd, num_args, arg_byte);
        this->write_command_(cmd, ptr, num_args);
        index += num_args;
        if (cmd == SLEEP_OUT)
          delay(10);
      }
    }
    // init sequence no longer needed
    this->init_sequence_.clear();
  }

  // Drawing operations

  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override {
    if (this->is_failed())
      return;
    if (w <= 0 || h <= 0)
      return;
    if (get_pixel_mode(bitness) != BUFFERPIXEL || big_endian != IS_BIG_ENDIAN) {
      // note that the usual logging macros are banned in header files, so use their replacement
      esph_log_e(TAG, "Unsupported color depth or bit order");
      return;
    }
    this->write_to_display_(x_start, y_start, w, h, reinterpret_cast<const BUFFERTYPE *>(ptr), x_offset, y_offset,
                            x_pad);
  }

  void dump_config() override {
    esph_log_config(TAG,
                    "MIPI_SPI Display\n"
                    "  Model: %s\n"
                    "  Width: %u\n"
                    "  Height: %u",
                    this->model_, WIDTH, HEIGHT);
    if constexpr (OFFSET_WIDTH != 0)
      esph_log_config(TAG, "  Offset width: %u", OFFSET_WIDTH);
    if constexpr (OFFSET_HEIGHT != 0)
      esph_log_config(TAG, "  Offset height: %u", OFFSET_HEIGHT);
    esph_log_config(TAG,
                    "  Swap X/Y: %s\n"
                    "  Mirror X: %s\n"
                    "  Mirror Y: %s\n"
                    "  Invert colors: %s\n"
                    "  Color order: %s\n"
                    "  Display pixels: %d bits\n"
                    "  Endianness: %s\n",
                    YESNO(this->madctl_ & MADCTL_MV), YESNO(this->madctl_ & (MADCTL_MX | MADCTL_XFLIP)),
                    YESNO(this->madctl_ & (MADCTL_MY | MADCTL_YFLIP)), YESNO(this->invert_colors_),
                    this->madctl_ & MADCTL_BGR ? "BGR" : "RGB", DISPLAYPIXEL * 8, IS_BIG_ENDIAN ? "Big" : "Little");
    if (this->brightness_.has_value())
      esph_log_config(TAG, "  Brightness: %u", this->brightness_.value());
    if (this->cs_ != nullptr)
      esph_log_config(TAG, "  CS Pin: %s", this->cs_->dump_summary().c_str());
    if (this->reset_pin_ != nullptr)
      esph_log_config(TAG, "  Reset Pin: %s", this->reset_pin_->dump_summary().c_str());
    if (this->dc_pin_ != nullptr)
      esph_log_config(TAG, "  DC Pin: %s", this->dc_pin_->dump_summary().c_str());
    esph_log_config(TAG,
                    "  SPI Mode: %d\n"
                    "  SPI Data rate: %dMHz\n"
                    "  SPI Bus width: %d",
                    this->mode_, static_cast<unsigned>(this->data_rate_ / 1000000), BUS_TYPE);
  }

 protected:
  /* METHODS */
  // convenience functions to write commands with or without data
  void write_command_(uint8_t cmd, uint8_t data) { this->write_command_(cmd, &data, 1); }
  void write_command_(uint8_t cmd) { this->write_command_(cmd, &cmd, 0); }

  // Writes a command to the display, with the given bytes.
  void write_command_(uint8_t cmd, const uint8_t *bytes, size_t len) {
    esph_log_v(TAG, "Command %02X, length %d, bytes %s", cmd, len, format_hex_pretty(bytes, len).c_str());
    if constexpr (BUS_TYPE == BUS_TYPE_QUAD) {
      this->enable();
      this->write_cmd_addr_data(8, 0x02, 24, cmd << 8, bytes, len);
      this->disable();
    } else if constexpr (BUS_TYPE == BUS_TYPE_OCTAL) {
      this->dc_pin_->digital_write(false);
      this->enable();
      this->write_cmd_addr_data(0, 0, 0, 0, &cmd, 1, 8);
      this->disable();
      this->dc_pin_->digital_write(true);
      if (len != 0) {
        this->enable();
        this->write_cmd_addr_data(0, 0, 0, 0, bytes, len, 8);
        this->disable();
      }
    } else if constexpr (BUS_TYPE == BUS_TYPE_SINGLE) {
      this->dc_pin_->digital_write(false);
      this->enable();
      this->write_byte(cmd);
      this->disable();
      this->dc_pin_->digital_write(true);
      if (len != 0) {
        this->enable();
        this->write_array(bytes, len);
        this->disable();
      }
    } else if constexpr (BUS_TYPE == BUS_TYPE_SINGLE_16) {
      this->dc_pin_->digital_write(false);
      this->enable();
      this->write_byte(cmd);
      this->disable();
      this->dc_pin_->digital_write(true);
      for (size_t i = 0; i != len; i++) {
        this->enable();
        this->write_byte(0);
        this->write_byte(bytes[i]);
        this->disable();
      }
    }
  }

  // write changed parameters to the display
  void reset_params_() {
    if (!this->is_ready())
      return;
    this->write_command_(this->invert_colors_ ? INVERT_ON : INVERT_OFF);
    if (this->brightness_.has_value())
      this->write_command_(BRIGHTNESS, this->brightness_.value());
  }

  // set the address window for the next data write
  void set_addr_window_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    esph_log_v(TAG, "Set addr %d/%d, %d/%d", x1, y1, x2, y2);
    uint8_t buf[4];
    x1 += OFFSET_WIDTH;
    x2 += OFFSET_WIDTH;
    y1 += OFFSET_HEIGHT;
    y2 += OFFSET_HEIGHT;
    put16_be(buf, y1);
    put16_be(buf + 2, y2);
    this->write_command_(RASET, buf, sizeof buf);
    put16_be(buf, x1);
    put16_be(buf + 2, x2);
    this->write_command_(CASET, buf, sizeof buf);
    if constexpr (BUS_TYPE != BUS_TYPE_QUAD) {
      this->write_command_(WDATA);
    }
  }

  // map the display color bitness to the pixel mode
  static PixelMode get_pixel_mode(display::ColorBitness bitness) {
    switch (bitness) {
      case display::COLOR_BITNESS_888:
        return PIXEL_MODE_18;  // 18 bits per pixel
      case display::COLOR_BITNESS_565:
        return PIXEL_MODE_16;  // 16 bits per pixel
      default:
        return PIXEL_MODE_8;  // Default to 8 bits per pixel
    }
  }

  /**
   * Writes a buffer to the display.
   * @param ptr The pointer to the pixel data
   * @param w Width of each line in bytes
   * @param h Height of the buffer in rows
   * @param pad Padding in bytes after each line
   */
  void write_display_data_(const uint8_t *ptr, size_t w, size_t h, size_t pad) {
    if (pad == 0) {
      if constexpr (BUS_TYPE == BUS_TYPE_SINGLE || BUS_TYPE == BUS_TYPE_SINGLE_16) {
        this->write_array(ptr, w * h);
      } else if constexpr (BUS_TYPE == BUS_TYPE_QUAD) {
        this->write_cmd_addr_data(8, 0x32, 24, WDATA << 8, ptr, w * h, 4);
      } else if constexpr (BUS_TYPE == BUS_TYPE_OCTAL) {
        this->write_cmd_addr_data(0, 0, 0, 0, ptr, w * h, 8);
      }
    } else {
      for (size_t y = 0; y != static_cast<size_t>(h); y++) {
        if constexpr (BUS_TYPE == BUS_TYPE_SINGLE || BUS_TYPE == BUS_TYPE_SINGLE_16) {
          this->write_array(ptr, w);
        } else if constexpr (BUS_TYPE == BUS_TYPE_QUAD) {
          this->write_cmd_addr_data(8, 0x32, 24, WDATA << 8, ptr, w, 4);
        } else if constexpr (BUS_TYPE == BUS_TYPE_OCTAL) {
          this->write_cmd_addr_data(0, 0, 0, 0, ptr, w, 8);
        }
        ptr += w + pad;
      }
    }
  }

  /**
   * Writes a buffer to the display.
   *
   * The ptr is a pointer to the pixel data
   * The other parameters are all in pixel units.
   */
  void write_to_display_(int x_start, int y_start, int w, int h, const BUFFERTYPE *ptr, int x_offset, int y_offset,
                         int x_pad) {
    this->set_addr_window_(x_start, y_start, x_start + w - 1, y_start + h - 1);
    this->enable();
    ptr += y_offset * (x_offset + w + x_pad) + x_offset;
    if constexpr (BUFFERPIXEL == DISPLAYPIXEL) {
      this->write_display_data_(reinterpret_cast<const uint8_t *>(ptr), w * sizeof(BUFFERTYPE), h,
                                x_pad * sizeof(BUFFERTYPE));
    } else {
      // type conversion required, do it in chunks
      uint8_t dbuffer[DISPLAYPIXEL * 48];
      uint8_t *dptr = dbuffer;
      auto stride = x_offset + w + x_pad;  // stride in pixels
      for (size_t y = 0; y != static_cast<size_t>(h); y++) {
        for (size_t x = 0; x != static_cast<size_t>(w); x++) {
          auto color_val = ptr[y * stride + x];
          if constexpr (DISPLAYPIXEL == PIXEL_MODE_18 && BUFFERPIXEL == PIXEL_MODE_16) {
            // 16 to 18 bit conversion
            if constexpr (IS_BIG_ENDIAN) {
              *dptr++ = color_val & 0xF8;
              *dptr++ = ((color_val & 0x7) << 5) | (color_val & 0xE000) >> 11;
              *dptr++ = (color_val >> 5) & 0xF8;
            } else {
              *dptr++ = (color_val >> 8) & 0xF8;  // Blue
              *dptr++ = (color_val & 0x7E0) >> 3;
              *dptr++ = color_val << 3;
            }
          } else if constexpr (DISPLAYPIXEL == PIXEL_MODE_18 && BUFFERPIXEL == PIXEL_MODE_8) {
            // 8 bit to 18 bit conversion
            *dptr++ = color_val << 6;           // Blue
            *dptr++ = (color_val & 0x1C) << 3;  // Green
            *dptr++ = (color_val & 0xE0);       // Red
          } else if constexpr (DISPLAYPIXEL == PIXEL_MODE_16 && BUFFERPIXEL == PIXEL_MODE_8) {
            if constexpr (IS_BIG_ENDIAN) {
              *dptr++ = (color_val & 0xE0) | ((color_val & 0x1C) >> 2);
              *dptr++ = (color_val & 3) << 3;
            } else {
              *dptr++ = (color_val & 3) << 3;
              *dptr++ = (color_val & 0xE0) | ((color_val & 0x1C) >> 2);
            }
          }
          // buffer full? Flush.
          if (dptr == dbuffer + sizeof(dbuffer)) {
            this->write_display_data_(dbuffer, sizeof(dbuffer), 1, 0);
            dptr = dbuffer;
          }
        }
      }
      // flush any remaining data
      if (dptr != dbuffer) {
        this->write_display_data_(dbuffer, dptr - dbuffer, 1, 0);
      }
    }
    this->disable();
  }

  /* PROPERTIES */

  // GPIO pins
  GPIOPin *reset_pin_{nullptr};
  std::vector<GPIOPin *> enable_pins_{};
  GPIOPin *dc_pin_{nullptr};

  // other properties set by configuration
  bool invert_colors_{};
  optional<uint8_t> brightness_{};
  const char *model_{"Unknown"};
  std::vector<uint8_t> init_sequence_{};
  uint8_t madctl_{};
};

/**
 * Class for MIPI SPI displays with a buffer.
 *
 * @tparam BUFFERTYPE The type of the buffer pixels, e.g. uint8_t or uint16_t
 * @tparam BUFFERPIXEL Color depth of the buffer
 * @tparam DISPLAYPIXEL Color depth of the display
 * @tparam BUS_TYPE The type of the interface bus (single, quad, octal)
 * @tparam ROTATION The rotation of the display
 * @tparam WIDTH Width of the display in pixels
 * @tparam HEIGHT Height of the display in pixels
 * @tparam OFFSET_WIDTH The x-offset of the display in pixels
 * @tparam OFFSET_HEIGHT The y-offset of the display in pixels
 * @tparam FRACTION The fraction of the display size to use for the buffer (e.g. 4 means a 1/4 buffer).
 * @tparam ROUNDING The alignment requirement for drawing operations (e.g. 2 means that x coordinates must be even)
 */
template<typename BUFFERTYPE, PixelMode BUFFERPIXEL, bool IS_BIG_ENDIAN, PixelMode DISPLAYPIXEL, BusType BUS_TYPE,
         uint16_t WIDTH, uint16_t HEIGHT, int OFFSET_WIDTH, int OFFSET_HEIGHT, display::DisplayRotation ROTATION,
         int FRACTION, unsigned ROUNDING>
class MipiSpiBuffer : public MipiSpi<BUFFERTYPE, BUFFERPIXEL, IS_BIG_ENDIAN, DISPLAYPIXEL, BUS_TYPE, WIDTH, HEIGHT,
                                     OFFSET_WIDTH, OFFSET_HEIGHT> {
 public:
  // these values define the buffer size needed to write in accordance with the chip pixel alignment
  // requirements. If the required rounding does not divide the width and height, we round up to the next multiple and
  // ignore the extra columns and rows when drawing, but use them to write to the display.
  static constexpr unsigned BUFFER_WIDTH = (WIDTH + ROUNDING - 1) / ROUNDING * ROUNDING;
  static constexpr unsigned BUFFER_HEIGHT = (HEIGHT + ROUNDING - 1) / ROUNDING * ROUNDING;

  MipiSpiBuffer() { this->rotation_ = ROTATION; }

  void dump_config() override {
    MipiSpi<BUFFERTYPE, BUFFERPIXEL, IS_BIG_ENDIAN, DISPLAYPIXEL, BUS_TYPE, WIDTH, HEIGHT, OFFSET_WIDTH,
            OFFSET_HEIGHT>::dump_config();
    esph_log_config(TAG,
                    "  Rotation: %d°\n"
                    "  Buffer pixels: %d bits\n"
                    "  Buffer fraction: 1/%d\n"
                    "  Buffer bytes: %zu\n"
                    "  Draw rounding: %u",
                    this->rotation_, BUFFERPIXEL * 8, FRACTION,
                    sizeof(BUFFERTYPE) * BUFFER_WIDTH * BUFFER_HEIGHT / FRACTION, ROUNDING);
  }

  void setup() override {
    MipiSpi<BUFFERTYPE, BUFFERPIXEL, IS_BIG_ENDIAN, DISPLAYPIXEL, BUS_TYPE, WIDTH, HEIGHT, OFFSET_WIDTH,
            OFFSET_HEIGHT>::setup();
    RAMAllocator<BUFFERTYPE> allocator{};
    this->buffer_ = allocator.allocate(BUFFER_WIDTH * BUFFER_HEIGHT / FRACTION);
    if (this->buffer_ == nullptr) {
      this->mark_failed(LOG_STR("Buffer allocation failed"));
    }
  }

  void update() override {
#if ESPHOME_LOG_LEVEL == ESPHOME_LOG_LEVEL_VERBOSE
    auto now = millis();
#endif
    if (this->is_failed()) {
      return;
    }
    // for updates with a small buffer, we repeatedly call the writer_ function, clipping the height to a fraction of
    // the display height,
    for (this->start_line_ = 0; this->start_line_ < HEIGHT; this->start_line_ += HEIGHT / FRACTION) {
#if ESPHOME_LOG_LEVEL == ESPHOME_LOG_LEVEL_VERBOSE
      auto lap = millis();
#endif
      this->end_line_ = this->start_line_ + HEIGHT / FRACTION;
      if (this->auto_clear_enabled_) {
        this->clear();
      }
      if (this->page_ != nullptr) {
        this->page_->get_writer()(*this);
      } else if (this->writer_.has_value()) {
        (*this->writer_)(*this);
      } else {
        this->test_card();
      }
#if ESPHOME_LOG_LEVEL == ESPHOME_LOG_LEVEL_VERBOSE
      esph_log_v(TAG, "Drawing from line %d took %dms", this->start_line_, millis() - lap);
      lap = millis();
#endif
      if (this->x_low_ > this->x_high_ || this->y_low_ > this->y_high_)
        return;
      esph_log_v(TAG, "x_low %d, y_low %d, x_high %d, y_high %d", this->x_low_, this->y_low_, this->x_high_,
                 this->y_high_);
      // Some chips require that the drawing window be aligned on certain boundaries
      this->x_low_ = this->x_low_ / ROUNDING * ROUNDING;
      this->y_low_ = this->y_low_ / ROUNDING * ROUNDING;
      this->x_high_ = (this->x_high_ + ROUNDING) / ROUNDING * ROUNDING - 1;
      this->y_high_ = (this->y_high_ + ROUNDING) / ROUNDING * ROUNDING - 1;
      int w = this->x_high_ - this->x_low_ + 1;
      int h = this->y_high_ - this->y_low_ + 1;
      this->write_to_display_(this->x_low_, this->y_low_, w, h, this->buffer_, this->x_low_,
                              this->y_low_ - this->start_line_, BUFFER_WIDTH - w);
      // invalidate watermarks
      this->x_low_ = WIDTH;
      this->y_low_ = HEIGHT;
      this->x_high_ = 0;
      this->y_high_ = 0;
#if ESPHOME_LOG_LEVEL == ESPHOME_LOG_LEVEL_VERBOSE
      esph_log_v(TAG, "Write to display took %dms", millis() - lap);
      lap = millis();
#endif
    }
#if ESPHOME_LOG_LEVEL == ESPHOME_LOG_LEVEL_VERBOSE
    esph_log_v(TAG, "Total update took %dms", millis() - now);
#endif
  }

  // Draw a pixel at the given coordinates.
  void draw_pixel_at(int x, int y, Color color) override {
    if (!this->get_clipping().inside(x, y))
      return;
    rotate_coordinates(x, y);
    if (x < 0 || x >= WIDTH || y < this->start_line_ || y >= this->end_line_)
      return;
    this->buffer_[(y - this->start_line_) * BUFFER_WIDTH + x] = convert_color(color);
    if (x < this->x_low_) {
      this->x_low_ = x;
    }
    if (x > this->x_high_) {
      this->x_high_ = x;
    }
    if (y < this->y_low_) {
      this->y_low_ = y;
    }
    if (y > this->y_high_) {
      this->y_high_ = y;
    }
  }

  // Fills the display with a color.
  void fill(Color color) override {
    this->x_low_ = 0;
    this->y_low_ = this->start_line_;
    this->x_high_ = WIDTH - 1;
    this->y_high_ = this->end_line_ - 1;
    std::fill_n(this->buffer_, HEIGHT * BUFFER_WIDTH / FRACTION, convert_color(color));
  }

  int get_width() override {
    if constexpr (ROTATION == display::DISPLAY_ROTATION_90_DEGREES || ROTATION == display::DISPLAY_ROTATION_270_DEGREES)
      return HEIGHT;
    return WIDTH;
  }

  int get_height() override {
    if constexpr (ROTATION == display::DISPLAY_ROTATION_90_DEGREES || ROTATION == display::DISPLAY_ROTATION_270_DEGREES)
      return WIDTH;
    return HEIGHT;
  }

 protected:
  // Rotate the coordinates to match the display orientation.
  static void rotate_coordinates(int &x, int &y) {
    if constexpr (ROTATION == display::DISPLAY_ROTATION_180_DEGREES) {
      x = WIDTH - x - 1;
      y = HEIGHT - y - 1;
    } else if constexpr (ROTATION == display::DISPLAY_ROTATION_90_DEGREES) {
      auto tmp = x;
      x = WIDTH - y - 1;
      y = tmp;
    } else if constexpr (ROTATION == display::DISPLAY_ROTATION_270_DEGREES) {
      auto tmp = y;
      y = HEIGHT - x - 1;
      x = tmp;
    }
  }

  // Convert a color to the buffer pixel format.
  static BUFFERTYPE convert_color(const Color &color) {
    if constexpr (BUFFERPIXEL == PIXEL_MODE_8) {
      return (color.red & 0xE0) | (color.g & 0xE0) >> 3 | color.b >> 6;
    } else if constexpr (BUFFERPIXEL == PIXEL_MODE_16) {
      if constexpr (IS_BIG_ENDIAN) {
        return (color.r & 0xF8) | color.g >> 5 | (color.g & 0x1C) << 11 | (color.b & 0xF8) << 5;
      } else {
        return (color.r & 0xF8) << 8 | (color.g & 0xFC) << 3 | color.b >> 3;
      }
    }
    return static_cast<BUFFERTYPE>(0);
  }

  BUFFERTYPE *buffer_{};
  uint16_t x_low_{WIDTH};
  uint16_t y_low_{HEIGHT};
  uint16_t x_high_{0};
  uint16_t y_high_{0};
  uint16_t start_line_{0};
  uint16_t end_line_{1};
};

}  // namespace mipi_spi
}  // namespace esphome
