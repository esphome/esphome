//
// Created by Clyde Stubbs on 29/10/2023.
//
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"
#ifdef USE_POWER_SUPPLY
#include "esphome/components/power_supply/power_supply.h"
#endif

#include "esp_lcd_panel_rgb.h"

namespace esphome {
namespace st7701s {

class ST7701S : public PollingComponent,
                public display::DisplayBuffer,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_10MHZ> {
 public:
  void setup() override {
    this->spi_setup();

    esp_lcd_rgb_panel_config_t config;
    config.timings.pclk_hz = 18 * 1000 * 1000;
    config.timings.h_res = this->get_width_internal();
    config.timings.v_res = this->get_height_internal();
    config.timings.hsync_pulse_width = 8;
    config.timings.hsync_back_porch = 50;
    config.timings.hsync_front_porch = 10;
    config.timings.vsync_pulse_width = 8;
    config.timings.vsync_back_porch = 20;
    config.timings.vsync_front_porch = 10;
    config.timings.flags.pclk_active_neg = 0;
    config.data_width = 16;
    config.data_gpio_nums[0] = 15;
    config.data_gpio_nums[1] = 14;
    config.data_gpio_nums[2] = 13;
    config.data_gpio_nums[3] = 12;
    config.data_gpio_nums[4] = 11;
    config.data_gpio_nums[5] = 10;
    config.data_gpio_nums[6] = 9;
    config.data_gpio_nums[7] = 8;
    config.data_gpio_nums[8] = 7;
    config.data_gpio_nums[9] = 6;
    config.data_gpio_nums[10] = 5;
    config.data_gpio_nums[11] = 4;
    config.data_gpio_nums[12] = 3;
    config.data_gpio_nums[13] = 2;
    config.data_gpio_nums[14] = 1;
    config.data_gpio_nums[15] = 0;
    config.sram_trans_align =
    config.clk_src = LCD_CLK_SRC_PLL160M;
    config.sram_trans_align = 4;
    config.psram_trans_align = 4;
    config.hsync_gpio_num = 16;
    config.vsync_gpio_num = 17;
    config.de_gpio_num = 18;
    config.pclk_gpio_num = 21;
    esp_lcd_new_rgb_panel(&config, &this->handle_);

  }

  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_backlight_pin(GPIOPin *backlight_pin) { this->backlight_pin_ = backlight_pin; }
#ifdef USE_POWER_SUPPLY
  void set_power_supply(power_supply::PowerSupply *power_supply) { this->power_.set_parent(power_supply); }
#endif
  void dump_config() override;

 protected:
  void write_command_(uint8_t value) {
    this->enable();
    this->dc_pin_->digital_write(false);
    this->write_byte(value);
    this->dc_pin_->digital_write(true);
    this->disable();
  }

  void write_data_(uint8_t value) {
    this->dc_pin_->digital_write(true);
    this->enable();
    this->write_byte(value);
    this->disable();
  }

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  GPIOPin *dc_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *backlight_pin_{nullptr};
#ifdef USE_POWER_SUPPLY
  power_supply::PowerSupplyRequester power_;
#endif

  uint16_t height_{0};
  uint16_t width_{0};
  uint16_t offset_height_{0};
  uint16_t offset_width_{0};

  esp_lcd_panel_handle_t handle_{};
};

}  // namespace st7701s
}  // namespace esphome
