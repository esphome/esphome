#pragma once

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/split_buffer/split_buffer.h"
#include "esphome/core/component.h"

#include <queue>

namespace esphome::epaper_spi {
using namespace display;

enum class EPaperState : uint8_t {
  IDLE,
  UPDATE,
  RESET,
  INITIALISE,
  TRANSFER_DATA,
  POWER_ON,
  REFRESH_SCREEN,
  POWER_OFF,
  DEEP_SLEEP,
};

static constexpr uint8_t MAX_TRANSFER_TIME = 10;  // Transfer in 10ms blocks to allow the loop to run
static constexpr uint8_t DELAY_FLAG = 0xFF;

class EPaperBase : public DisplayBuffer,
                   public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                         spi::DATA_RATE_2MHZ> {
 public:
  EPaperBase(uint16_t width, uint16_t height, const uint8_t *init_sequence, const size_t init_sequence_length,
             DisplayType display_type = DISPLAY_TYPE_BINARY)
      : width_(width),
        height_(height),
        init_sequence_(init_sequence),
        init_sequence_length_(init_sequence_length),
        display_type_(display_type) {}
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }
  float get_setup_priority() const override;
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_busy_pin(GPIOPin *busy) { this->busy_pin_ = busy; }
  void set_reset_duration(uint32_t reset_duration) { this->reset_duration_ = reset_duration; }

  void command(uint8_t value);
  void data(uint8_t value);
  void cmd_data(uint8_t command, const uint8_t *ptr, size_t length);

  void update() override;
  void loop() override;

  void setup() override;

  void on_safe_shutdown() override;

  DisplayType get_display_type() override { return this->display_type_; };

 protected:
  int get_height_internal() override { return this->height_; };
  int get_width_internal() override { return this->width_; };

  bool is_idle_() const;
  void setup_pins_() const;
  virtual void reset();
  void initialise_();
  bool init_buffer_(size_t buffer_length);

  virtual int get_width_controller() { return this->get_width_internal(); };
  virtual void deep_sleep() = 0;
  /**
   * Send data to the device via SPI
   * @return true if done, false if it should be called next loop
   */
  virtual bool transfer_data() = 0;
  virtual void refresh_screen() = 0;

  virtual void power_on() = 0;
  virtual void power_off() = 0;
  virtual uint32_t get_buffer_length() = 0;

  void start_command_();
  void end_command_();
  void start_data_();
  void end_data_();

  // properties initialised in the constructor
  uint16_t width_;
  uint16_t height_;
  const uint8_t *init_sequence_;
  const size_t init_sequence_length_;
  DisplayType display_type_;

  size_t current_data_index_{0};
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  uint32_t transfer_start_time_{};
#endif
  uint32_t reset_duration_{200};
  uint32_t waiting_for_idle_last_print_{0};

  GPIOPin *dc_pin_{};
  GPIOPin *busy_pin_{};
  GPIOPin *reset_pin_{};

  bool waiting_for_idle_{false};

  split_buffer::SplitBuffer buffer_;

  std::queue<EPaperState> state_queue_{{EPaperState::IDLE}};
};

}  // namespace esphome::epaper_spi
