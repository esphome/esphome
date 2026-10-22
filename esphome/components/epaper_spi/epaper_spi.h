#pragma once

#include "colorconv.h"
#include "esphome/components/display/display.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/split_buffer/split_buffer.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::epaper_spi {
using namespace display;

enum class EPaperState : uint8_t {
  IDLE,       // not doing anything
  UPDATE,     // update the buffer
  RESET,      // drive reset low (active)
  RESET_END,  // drive reset high (inactive)

  SHOULD_WAIT,     // states higher than this should wait for the display to be not busy
  INITIALISE,      // send the init sequence
  TRANSFER_DATA,   // transfer data to the display
  POWER_ON,        // power on the display
  REFRESH_SCREEN,  // send refresh command
  POWER_OFF,       // power off the display
  DEEP_SLEEP,      // deep sleep the display
};

static constexpr uint8_t NONE = 0;
static constexpr uint8_t MIRROR_X = 1;
static constexpr uint8_t MIRROR_Y = 2;
static constexpr uint8_t SWAP_XY = 4;

static constexpr uint32_t MAX_TRANSFER_TIME = 10;  // Transfer in 10ms blocks to allow the loop to run
static constexpr size_t MAX_TRANSFER_SIZE = 128;
// Longer busy waits are the waveform or a stuck pin, and are polled at the normal loop pace
static constexpr uint32_t FAST_BUSY_POLL_MS = 50;
static constexpr uint8_t DELAY_FLAG = 0xFF;

class EPaperBase : public Display,
                   public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                         spi::DATA_RATE_2MHZ> {
 public:
  EPaperBase(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence = nullptr,
             size_t init_sequence_length = 0, DisplayType display_type = DISPLAY_TYPE_BINARY,
             uint8_t bits_per_pixel = 1, uint8_t planes = 1)
      : name_(name),
        width_(width),
        row_width_((width * bits_per_pixel + 7) / 8),
        height_(height),
        bits_per_pixel_(bits_per_pixel),
        planes_(planes),
        init_sequence_(init_sequence),
        init_sequence_length_(init_sequence_length),
        display_type_(display_type),
        buffer_length_((size_t) this->row_width_ * height * planes) {}
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }
  float get_setup_priority() const override;
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_busy_pin(GPIOPin *busy) { this->busy_pin_ = busy; }
  void set_enable_pins(std::vector<GPIOPin *> enable_pins) { this->enable_pins_ = std::move(enable_pins); }
  void set_reset_duration(uint32_t reset_duration) { this->reset_duration_ = reset_duration; }
  void set_transform(uint8_t transform) {
    this->transform_ = transform;
    this->update_effective_transform_();
  }
  void set_rotation(DisplayRotation rotation) override {
    Display::set_rotation(rotation);
    this->update_effective_transform_();
  }
  void set_full_update_every(uint8_t full_update_every) { this->full_update_every_ = full_update_every; }
  void dump_config() override;

  void command(uint8_t value);
  void cmd_data(uint8_t command, const uint8_t *ptr, size_t length);

  // variant with in-place initializer list
  void cmd_data(uint8_t command, std::initializer_list<uint8_t> data) {
    this->cmd_data(command, data.begin(), data.size());
  }

  void update() override;
  void loop() override;

  void setup() override;

  void on_safe_shutdown() override;

  DisplayType get_display_type() override { return this->display_type_; };

  // Default implementation for monochrome displays
  void fill(Color color) override {
    // If clipping is active, fall back to base implementation
    if (this->get_clipping().is_set()) {
      Display::fill(color);
      return;
    }

    auto pixel_color = color_to_mono(color) ? 0xFF : 0x00;

    // We store 8 pixels per byte
    this->buffer_.fill(pixel_color);
    this->x_high_ = this->width_;
    this->y_high_ = this->height_;
    this->x_low_ = 0;
    this->y_low_ = 0;
  }

  void clear() override {
    // clear buffer to white, just like real paper.
    this->fill(COLOR_ON);
  }

  int get_width() override { return this->effective_transform_ & SWAP_XY ? this->height_ : this->width_; }
  int get_height() override { return this->effective_transform_ & SWAP_XY ? this->width_ : this->height_; }
  bool pixel_axes_swapped() const override { return this->effective_transform_ & SWAP_XY; }
  void draw_pixel_at(int x, int y, Color color) override;
  // Make the next update a full one. Applied when that update starts, so one in progress is not affected.
  void request_full_update() { this->full_update_requested_ = true; }
  // True from the start of an update until the display has refreshed and gone back to sleep.
  bool is_updating() const { return this->state_ != EPaperState::IDLE; }

 protected:
  void draw_pixels_internal(int x_start, int y_start, int w, int h, const uint8_t *ptr, ColorBitness bitness,
                            bool big_endian, int x_offset, int y_offset, int x_pad) override;

  int get_height_internal() override { return this->height_; };
  int get_width_internal() override { return this->width_; };
  bool is_using_partial_update_() const { return this->full_update_every_ > 1; }
  void process_state_();

  const char *epaper_state_to_string_();
  virtual bool is_idle() const;
  void setup_pins_() const;
  virtual bool reset();
  /// The reset pin part of reset(), for a driver that must skip what its parent adds to it
  bool hardware_reset_();
  virtual bool initialise(bool partial);
  void send_init_sequence_(const uint8_t *sequence, size_t length);
  void wait_for_idle_(bool should_wait);
  void abandon_update_();
  bool init_buffer_(size_t buffer_length);
  void update_effective_transform_();
  bool rotate_coordinates_(int &x, int &y);
  void draw_rgb565_block_(uint8_t *buffer, int x_start, int y_start, int w, int h, const uint8_t *ptr, bool big_endian,
                          int x_offset, int y_offset, int x_pad);
  // Whether the buffer is a single plane of 1 bit per pixel, as draw_pixel_at() writes it
  bool buffer_is_mono_() const { return this->bits_per_pixel_ == 1 && this->planes_ == 1; }

  /**
   * Methods that must be implemented by concrete classes to control the display
   */
  /**
   * Send data to the device via SPI
   * @return true if done, false if it should be called next loop
   */
  virtual bool transfer_data() = 0;
  // Called when an update is abandoned in the middle of transfer_data(), to release anything it left open
  virtual void abort_transfer() {}
  /**
   * Refresh the screen after data transfer
   */
  virtual void refresh_screen(bool partial) = 0;

  /**
   * Power the display on
   */
  virtual void power_on() = 0;
  /**
   * Power the display off
   */
  virtual void power_off() = 0;

  /**
   * Called after the refresh, before power off, repeatedly until it returns true.
   * Lets a driver do extra work spread over several loops, such as rewriting the previous image.
   * @return true when done, false to be called again next loop
   */
  virtual bool after_refresh() { return true; }

  /**
   * Place the display into deep sleep
   */
  virtual void deep_sleep() = 0;

  void set_state_(EPaperState state, uint16_t delay = 0);

  void start_data_();

  // properties initialised in the constructor
  const char *name_;
  uint16_t width_;
  uint16_t row_width_;  // width of a row of one plane in bytes
  uint16_t height_;
  uint8_t bits_per_pixel_;
  uint8_t planes_;  // the buffer holds this many planes of row_width_ * height_ bytes, one after another
  const uint8_t *init_sequence_;
  size_t init_sequence_length_;
  DisplayType display_type_;

  size_t buffer_length_;
  size_t current_data_index_{};  // used by data transfer to track progress
  split_buffer::SplitBuffer buffer_{};
  GPIOPin *dc_pin_{};
  GPIOPin *busy_pin_{};
  GPIOPin *reset_pin_{};
  std::vector<GPIOPin *> enable_pins_{};
  bool waiting_for_idle_{};
  HighFrequencyLoopRequester high_frequency_loop_;
  uint32_t delay_until_{};  // timestamp until which to delay processing
  uint16_t next_delay_{};   // milliseconds to delay before next state
  uint8_t transform_{};
  uint8_t effective_transform_{};
  uint8_t update_count_{};
  bool full_update_requested_{};
  bool partial_{};  // the update in progress is a partial one; set when UPDATE moves to RESET
  // these values represent the bounds of the updated buffer. Note that x_high and y_high
  // point to the pixel past the last one updated, i.e. may range up to width/height.
  uint16_t x_low_{}, y_low_{}, x_high_{}, y_high_{};

  uint32_t waiting_for_idle_start_{};
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  uint32_t waiting_for_idle_last_print_{};
#endif
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
  uint32_t update_start_time_{};
#endif

  // properties with specific initialisers go last
  uint32_t reset_duration_{10};
  // Longer than any panel takes to refresh; past it the busy pin is assumed stuck
  uint16_t busy_timeout_seconds_{120};
  uint8_t full_update_every_{1};
  EPaperState state_{EPaperState::IDLE};
};

}  // namespace esphome::epaper_spi
