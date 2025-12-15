#pragma once

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/split_buffer/split_buffer.h"
#include "esphome/core/component.h"

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
static constexpr uint8_t DELAY_FLAG = 0xFF;

class EPaperBase : public Display,
                   public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                         spi::DATA_RATE_2MHZ> {
 public:
  EPaperBase(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
             size_t init_sequence_length, DisplayType display_type = DISPLAY_TYPE_BINARY)
      : name_(name),
        width_(width),
        height_(height),
        init_sequence_(init_sequence),
        init_sequence_length_(init_sequence_length),
        display_type_(display_type) {}
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }
  float get_setup_priority() const override;
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_busy_pin(GPIOPin *busy) { this->busy_pin_ = busy; }
  void set_reset_duration(uint32_t reset_duration) { this->reset_duration_ = reset_duration; }
  void set_transform(uint8_t transform) { this->transform_ = transform; }
  void set_full_update_every(uint8_t full_update_every) { this->full_update_every_ = full_update_every; }
  void dump_config() override;

  void command(uint8_t value);
  void data(uint8_t value);
  void cmd_data(uint8_t command, const uint8_t *ptr, size_t length);

  void update() override;
  void loop() override;

  void setup() override;

  void on_safe_shutdown() override;

  DisplayType get_display_type() override { return this->display_type_; };

  // Default implementations for monochrome displays
  static uint8_t color_to_bit(Color color) {
    // It's always a shade of gray. Map to BLACK or WHITE.
    // We split the luminance at a suitable point
    if ((static_cast<int>(color.r) + color.g + color.b) > 512) {
      return 1;
    }
    return 0;
  }
  void fill(Color color) override {
    auto pixel_color = color_to_bit(color) ? 0xFF : 0x00;

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

 protected:
  int get_height_internal() override { return this->height_; };
  int get_width_internal() override { return this->width_; };
  int get_width() override { return this->transform_ & SWAP_XY ? this->height_ : this->width_; }
  int get_height() override { return this->transform_ & SWAP_XY ? this->width_ : this->height_; }
  void draw_pixel_at(int x, int y, Color color) override;
  void process_state_();

  const char *epaper_state_to_string_();
  bool is_idle_() const;
  void setup_pins_() const;
  virtual bool reset();
  void initialise_();
  void wait_for_idle_(bool should_wait);
  bool init_buffer_(size_t buffer_length);
  bool rotate_coordinates_(int &x, int &y);

  /**
   * Methods that must be implemented by concrete classes to control the display
   */
  /**
   * Send data to the device via SPI
   * @return true if done, false if it should be called next loop
   */
  virtual bool transfer_data() = 0;
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
   * Place the display into deep sleep
   */
  virtual void deep_sleep() = 0;

  void set_state_(EPaperState state, uint16_t delay = 0);

  void start_command_();
  void end_command_();
  void start_data_();
  void end_data_();

  // properties initialised in the constructor
  const char *name_;
  uint16_t width_;
  uint16_t height_;
  const uint8_t *init_sequence_;
  size_t init_sequence_length_;
  DisplayType display_type_;

  size_t buffer_length_{};
  size_t current_data_index_{};  // used by data transfer to track progress
  split_buffer::SplitBuffer buffer_{};
  GPIOPin *dc_pin_{};
  GPIOPin *busy_pin_{};
  GPIOPin *reset_pin_{};
  bool waiting_for_idle_{};
  uint32_t delay_until_{};
  uint8_t transform_{};
  uint8_t update_count_{};
  // these values represent the bounds of the updated buffer. Note that x_high and y_high
  // point to the pixel past the last one updated, i.e. may range up to width/height.
  uint16_t x_low_{}, y_low_{}, x_high_{}, y_high_{};

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  uint32_t waiting_for_idle_last_print_{};
  uint32_t waiting_for_idle_start_{};
#endif
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
  uint32_t update_start_time_{};
#endif

  // properties with specific initialisers go last
  EPaperState state_{EPaperState::IDLE};
  uint32_t reset_duration_{10};
  uint8_t full_update_every_{1};
};

}  // namespace esphome::epaper_spi
