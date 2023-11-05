//
// Created by Clyde Stubbs on 29/10/2023.
//
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/panel_driver/panel_driver.h"
#ifdef USE_POWER_SUPPLY
#include "esphome/components/power_supply/power_supply.h"
#endif

#include "esp_lcd_panel_rgb.h"

namespace esphome {
namespace st7701s {

class ST7701S : public Component,
                public panel_driver::PanelDriver,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_1MHZ> {
 public:
  void setup() override {
    this->spi_setup();

    /*
    esp_lcd_rgb_panel_config_t config{};
    config.flags.fb_in_psram = 1;
    config.num_fbs = 1;
    config.timings.pclk_hz = 4 * 1000 * 1000;
    config.timings.h_res = this->width_;
    config.timings.v_res = this->height_;
    config.timings.hsync_pulse_width = this->hsync_pulse_width_;
    config.timings.hsync_back_porch = this->hsync_back_porch_;
    config.timings.hsync_front_porch = this->hsync_front_porch_;
    config.timings.vsync_pulse_width = this->vsync_pulse_width_;
    config.timings.vsync_back_porch = this->vsync_back_porch_;
    config.timings.vsync_front_porch = this->vsync_front_porch_;
    config.timings.flags.pclk_active_neg = 0;
    config.clk_src = LCD_CLK_SRC_PLL160M;
    config.sram_trans_align = 64;
    config.psram_trans_align = 64;
    size_t data_pin_count = sizeof(this->data_pins_) / sizeof(this->data_pins_[0]);
    for (size_t i = 0; i != data_pin_count; i++) {
      if (this->data_pins_[i] == nullptr) {
        esph_log_e("st7701s", "Data pin %u is not defined", i);
        return;
      }
      config.data_gpio_nums[i] = this->data_pins_[i]->get_pin();
    }
    config.data_width = data_pin_count;
    config.hsync_gpio_num = this->hsync_pin_->get_pin();
    config.vsync_gpio_num = this->vsync_pin_->get_pin();
    config.de_gpio_num = this->de_pin_->get_pin();
    config.pclk_gpio_num = this->pclk_pin_->get_pin();
    esp_err_t err = esp_lcd_new_rgb_panel(&config, &this->handle_);
    if (err != ESP_OK) {
      esph_log_e("st7701s", "lcd_new_rgb_panel failed: %s", esp_err_to_name(err));
    }
     */
    this->write_init_sequence();
  }

  void draw_pixels_in_window() override {}

  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void add_data_pin(InternalGPIOPin *data_pin, size_t index) { this->data_pins_[index] = data_pin; };
  void set_de_pin(InternalGPIOPin *de_pin) { this->de_pin_ = de_pin; }
  void set_pclk_pin(InternalGPIOPin *pclk_pin) { this->pclk_pin_ = pclk_pin; }
  void set_vsync_pin(InternalGPIOPin *vsync_pin) { this->vsync_pin_ = vsync_pin; }
  void set_hsync_pin(InternalGPIOPin *hsync_pin) { this->hsync_pin_ = hsync_pin; }
  void set_backlight_pin(GPIOPin *backlight_pin) { this->backlight_pin_ = backlight_pin; }
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_width(uint16_t width) { this->width_ = width; }
  void set_height(uint16_t height) { this->height_ = height; }
  void set_hsync_back_porch(uint16_t hsync_back_porch) { this->hsync_back_porch_ = hsync_back_porch; }
  void set_hsync_front_porch(uint16_t hsync_front_porch) { this->hsync_front_porch_ = hsync_front_porch; }
  void set_vsync_pulse_width(uint16_t vsync_pulse_width) { this->vsync_pulse_width_ = vsync_pulse_width; }
  void set_vsync_back_porch(uint16_t vsync_back_porch) { this->vsync_back_porch_ = vsync_back_porch; }
  void set_vsync_front_porch(uint16_t vsync_front_porch) { this->vsync_front_porch_ = vsync_front_porch; }
  void set_init_sequence(const std::vector<uint8_t> &init_sequence) { this->init_sequence_ = init_sequence; }
#ifdef USE_POWER_SUPPLY
  void set_power_supply(power_supply::PowerSupply *power_supply) { this->power_.set_parent(power_supply); }
#endif
  void dump_config() override;

 protected:
  void write_command_(uint8_t value) {
    this->enable();
    if (this->dc_pin_ == nullptr) {
      this->write(value, 9);
    } else {
      this->dc_pin_->digital_write(false);
      this->write_byte(value);
      this->dc_pin_->digital_write(true);
    }
    this->disable();
  }

  void write_data_(uint8_t value) {
    this->enable();
    if (this->dc_pin_ == nullptr) {
      this->write(value | 0x100, 9);
    } else {
      this->dc_pin_->digital_write(true);
      this->write_byte(value);
    }
    this->disable();
  }

  /**
   * this relies upon the init sequence being well-formed, which is guaranteed by the Python init code.
   */
  void write_init_sequence() {
    for (size_t i = 0; i != this->init_sequence_.size();) {
      this->write_command_(this->init_sequence_[i++]);
      size_t len = this->init_sequence_[i++];
      while (len-- != 0)
        this->write_data_(this->init_sequence_[i++]);
    }
    this->set_timeout(120, [this] { this->write_command_(0x29); });
  }

  InternalGPIOPin *de_pin_{nullptr};
  InternalGPIOPin *pclk_pin_{nullptr};
  InternalGPIOPin *hsync_pin_{nullptr};
  InternalGPIOPin *vsync_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *backlight_pin_{nullptr};
  GPIOPin *dc_pin_{nullptr};
  InternalGPIOPin *data_pins_[16] = {};
  uint16_t hsync_pulse_width_ = 8;
  uint16_t hsync_back_porch_ = 50;
  uint16_t hsync_front_porch_ = 10;
  uint16_t vsync_pulse_width_ = 8;
  uint16_t vsync_back_porch_ = 20;
  uint16_t vsync_front_porch_ = 10;
  std::vector<uint8_t> init_sequence_;
#ifdef USE_POWER_SUPPLY
  power_supply::PowerSupplyRequester power_;
#endif

  uint16_t height_{0};
  uint16_t width_{0};

  esp_lcd_panel_handle_t handle_{};
};

}  // namespace st7701s
}  // namespace esphome
