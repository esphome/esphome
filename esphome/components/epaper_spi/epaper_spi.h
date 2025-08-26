#pragma once

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/split_buffer/split_buffer.h"
#include "esphome/core/component.h"

namespace esphome::epaper_spi {

enum EPaperState : uint8_t {
  IDLE,
  UPDATING,
  RESETTING,
  RESET_DONE,
  INITIALIZING,
  TRANSFERING_DATA,
  TRANSFER_DONE,
  POWERING_ON,
  REFRESHING_SCREEN,
  POWERING_OFF,
};

class EPaperBase : public display::DisplayBuffer,
                   public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                         spi::DATA_RATE_2MHZ> {
 public:
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }
  float get_setup_priority() const override;
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_busy_pin(GPIOPin *busy) { this->busy_pin_ = busy; }
  void set_reset_duration(uint32_t reset_duration) { this->reset_duration_ = reset_duration; }

  void command(uint8_t value);
  void data(uint8_t value);
  void cmd_data(const uint8_t *data, size_t length);

  void update() override;
  void loop() override;

  void setup() override;

  void on_safe_shutdown() override;

 protected:
  bool is_idle_();
  void setup_pins_();
  void reset_();

  virtual int get_width_controller() { return this->get_width_internal(); };
  virtual void initialize() = 0;
  virtual void deep_sleep() = 0;
  virtual void transfer_data() = 0;
  virtual void refresh_screen() = 0;

  virtual void power_on() = 0;
  virtual void power_off() = 0;
  virtual uint32_t get_buffer_length() = 0;

  void start_command_();
  void end_command_();
  void start_data_();
  void end_data_();

  GPIOPin *dc_pin_;
  GPIOPin *busy_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};

  uint32_t reset_duration_{200};

  EPaperState state_{IDLE};

  split_buffer::SplitBuffer buffer_;
};

class EPaper6Color : public EPaperBase {
 public:
  uint8_t color_to_hex(Color color);
  void fill(Color color) override;

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  uint32_t get_buffer_length() override;
  void setup() override;
  bool init_internal_6c_(uint32_t buffer_length);
};

class EPaper7p3InE : public EPaper6Color {
 public:
  void initialize() override;

  void dump_config() override;

 protected:
  int get_width_internal() override { return 800; };
  int get_height_internal() override { return 480; };
  void transfer_data() override;
  void refresh_screen() override;

  void power_on() override;
  void power_off() override;

  void deep_sleep() override;

  size_t current_data_index_{0};
};

}  // namespace esphome::epaper_spi
