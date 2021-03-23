#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace waveshare_epaper {

class WaveshareEPaper : public PollingComponent,
                        public display::DisplayBuffer,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                              spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }
  float get_setup_priority() const override;
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_busy_pin(GPIOPin *busy) { this->busy_pin_ = busy; }

  void command(uint8_t value);
  void data(uint8_t value);

  virtual void display() = 0;
  virtual void initialize() = 0;
  virtual void deep_sleep() = 0;

  void update() override;

  virtual void fill(Color color) override;

  void setup() override {
    this->setup_pins_();
    this->initialize();
  }

  void on_safe_shutdown() override;

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  virtual bool wait_until_idle_();

  void setup_pins_();

  void reset_() {
    if (this->reset_pin_ != nullptr) {
      this->reset_pin_->digital_write(false);
      delay(200);  // NOLINT
      this->reset_pin_->digital_write(true);
      delay(200);  // NOLINT
    }
  }

  virtual uint32_t get_buffer_length_();

  void start_command_();
  void end_command_();
  void start_data_();
  void end_data_();

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *dc_pin_;
  GPIOPin *busy_pin_{nullptr};
  virtual int idle_timeout_() { return 1000; }  // NOLINT(readability-identifier-naming)
};

enum WaveshareEPaperTypeAModel {
  WAVESHARE_EPAPER_1_54_IN = 0,
  WAVESHARE_EPAPER_2_13_IN,
  WAVESHARE_EPAPER_2_9_IN,
  WAVESHARE_EPAPER_2_9_IN_V2,
  TTGO_EPAPER_2_13_IN,
  TTGO_EPAPER_2_13_IN_B73,
  TTGO_EPAPER_2_13_IN_B1,
};

class WaveshareEPaperTypeA : public WaveshareEPaper {
 public:
  WaveshareEPaperTypeA(WaveshareEPaperTypeAModel model);

  void initialize() override;

  void dump_config() override;

  void display() override;

  void deep_sleep() override {
    if (this->model_ == WAVESHARE_EPAPER_2_9_IN_V2) {
      // COMMAND DEEP SLEEP MODE
      this->command(0x10);
      this->data(0x01);
    } else {
      // COMMAND DEEP SLEEP MODE
      this->command(0x10);
    }
    this->wait_until_idle_();
  }

  void set_full_update_every(uint32_t full_update_every);

 protected:
  void write_lut_(const uint8_t *lut, uint8_t size);

  int get_width_internal() override;

  int get_height_internal() override;

  uint32_t full_update_every_{30};
  uint32_t at_update_{0};
  WaveshareEPaperTypeAModel model_;
  int idle_timeout_() override;
};

enum WaveshareEPaperTypeBModel {
  WAVESHARE_EPAPER_2_7_IN = 0,
  WAVESHARE_EPAPER_4_2_IN,
  WAVESHARE_EPAPER_7_5_IN,
  WAVESHARE_EPAPER_7_5_INV2,
};

class WaveshareEPaper2P7In : public WaveshareEPaper {
 public:
  void initialize() override;

  void display() override;

  void dump_config() override;

  void deep_sleep() override {
    // COMMAND DEEP SLEEP
    this->command(0x07);
    this->data(0xA5);  // check byte
  }

 protected:
  int get_width_internal() override;

  int get_height_internal() override;
};

class WaveshareEPaper2P9InB : public WaveshareEPaper {
 public:
  void initialize() override;

  void display() override;

  void dump_config() override;

  void deep_sleep() override {
    // COMMAND DEEP SLEEP
    this->command(0x07);
    this->data(0xA5);  // check byte
  }

 protected:
  int get_width_internal() override;

  int get_height_internal() override;
};

class WaveshareEPaper4P2In : public WaveshareEPaper {
 public:
  void initialize() override;

  void display() override;

  void dump_config() override;

  void deep_sleep() override {
    // COMMAND VCOM AND DATA INTERVAL SETTING
    this->command(0x50);
    this->data(0x17);  // border floating

    // COMMAND VCM DC SETTING
    this->command(0x82);
    // COMMAND PANEL SETTING
    this->command(0x00);

    delay(100);  // NOLINT

    // COMMAND POWER SETTING
    this->command(0x01);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    this->data(0x00);
    delay(100);  // NOLINT

    // COMMAND POWER OFF
    this->command(0x02);
    this->wait_until_idle_();
    // COMMAND DEEP SLEEP
    this->command(0x07);
    this->data(0xA5);  // check byte
  }

 protected:
  int get_width_internal() override;

  int get_height_internal() override;
};

class WaveshareEPaper5P8In : public WaveshareEPaper {
 public:
  void initialize() override;

  void display() override;

  void dump_config() override;

  void deep_sleep() override {
    // COMMAND POWER OFF
    this->command(0x02);
    this->wait_until_idle_();
    // COMMAND DEEP SLEEP
    this->command(0x07);
    this->data(0xA5);  // check byte
  }

 protected:
  int get_width_internal() override;

  int get_height_internal() override;
};

class WaveshareEPaper7P5In : public WaveshareEPaper {
 public:
  void initialize() override;

  void display() override;

  void dump_config() override;

  void deep_sleep() override {
    // COMMAND POWER OFF
    this->command(0x02);
    this->wait_until_idle_();
    // COMMAND DEEP SLEEP
    this->command(0x07);
    this->data(0xA5);  // check byte
  }

 protected:
  int get_width_internal() override;

  int get_height_internal() override;
};

class WaveshareEPaper7P5InV2 : public WaveshareEPaper {
 public:
  void initialize() override;

  void display() override;

  void dump_config() override;

  void deep_sleep() override {
    // COMMAND POWER OFF
    this->command(0x02);
    this->wait_until_idle_();
    // COMMAND DEEP SLEEP
    this->command(0x07);
    this->data(0xA5);  // check byte
  }

 protected:
  int get_width_internal() override;

  int get_height_internal() override;
};

enum WaveshareEPaperTypeFModel {
  WAVESHARE_ACEP_5_65_IN = 0,
};

static const Color COLOR_F_BLACK(0, 0, 0);
static const Color COLOR_F_WHITE(255, 255, 255);
static const Color COLOR_F_GREEN(0, 255, 0);
static const Color COLOR_F_BLUE(0, 0, 255);
static const Color COLOR_F_RED(255, 0, 0);
static const Color COLOR_F_YELLOW(255, 255, 0);
static const Color COLOR_F_ORANGE(255, 127, 0);
static const Color COLOR_F_CLEAN(0x123456);  // Anything that isn't one of the above will do.

class WaveshareEPaperTypeF : public WaveshareEPaper {
 public:
  WaveshareEPaperTypeF(WaveshareEPaperTypeFModel model);

  void initialize() override;

  void dump_config() override;

  void display() override;

  void fill(Color color) override;

  void deep_sleep() override {
    if (this->reset_pin_ != nullptr) {
      delay(100);  // NOLINT
      this->command(0x07);
      this->data(0xA5);
      delay(100);  // NOLINT
      this->reset_pin_->digital_write(false);
    }
  }

 protected:
  virtual uint8_t color_(Color color);  // NOLINT(readability-identifier-naming)

  uint8_t pixel_storage_size_ = 3;

  void draw_absolute_pixel_internal(int x, int y, Color color) override;  // NOLINT(readability-identifier-naming)
  void draw_absolute_pixel_internal(int x, int y, uint8_t index);         // NOLINT(readability-identifier-naming)
  uint8_t get_index_value_(uint32_t pos);
  void send_display_size_(uint16_t width, uint16_t height);  // NOLINT(readability-identifier-naming)

  int get_width_internal() override;
  int get_height_internal() override;
  uint32_t get_buffer_length_() override;  // NOLINT(readability-identifier-naming)

  bool wait_until_idle_() override;  // NOLINT(readability-identifier-naming)
  bool wait_until_busy_();           // NOLINT(readability-identifier-naming)

  WaveshareEPaperTypeFModel model_;
};

}  // namespace waveshare_epaper
}  // namespace esphome
