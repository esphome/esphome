//
// bb_epaper wrapper library for ESPHome
//
// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Larry Bank <bitbank@pobox.com>
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===========================================================================
//
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"
#include "bb_epaper.h"

namespace esphome {
namespace bb_epaper {

class bb_epaper : public display::DisplayBuffer,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                        spi::DATA_RATE_8MHZ> {
 public:
  bb_epaper(void) { ESP_LOGCONFIG("bb_epaper class", "instantiation"); }
  void set_model(std::string model) { model_name = model; }
  void set_refresh_type(std::string type) { refresh_type = type; }
  void set_dc_pin(InternalGPIOPin *dc_pin) { dc_pin_ = dc_pin; }
  void set_cs_pin(InternalGPIOPin *cs_pin) { cs_pin_ = cs_pin; }
  void set_power_pin(InternalGPIOPin *power_pin) { power_pin_ = power_pin; }
  float get_setup_priority() const override;
  void set_reset_pin(InternalGPIOPin *reset) { this->reset_pin_ = reset; }
  void set_busy_pin(InternalGPIOPin *busy) { this->busy_pin_ = busy; }

  void command(uint8_t value);
  void data(uint8_t value);
  void cmd_data(const uint8_t *data, size_t length);

  void fill(Color color) override;

  void update() override;

  void setup() override;

  void on_safe_shutdown() override;
  void dump_config() override;

  void deep_sleep() { _bbepaper.sleep(LIGHT_SLEEP); }

  display::DisplayType get_display_type() override {
    ESP_LOGCONFIG("bb_epaper", "get_display_type() ");
    return display::DisplayType::DISPLAY_TYPE_BINARY;
  }
  BBEPAPER _bbepaper;
  InternalGPIOPin *reset_pin_{nullptr};
  InternalGPIOPin *dc_pin_;
  InternalGPIOPin *cs_pin_;
  InternalGPIOPin *power_pin_;
  InternalGPIOPin *busy_pin_{nullptr};
  std::string model_name, refresh_type;

 protected:
  int get_height_internal() override;
  int get_width_internal() override;
  void display();
  void setup_pins_();
  int _refresh;  // refresh type
  int iCount;

  virtual int get_width_controller() { return this->get_width_internal(); };

  virtual uint32_t idle_timeout_() { return 1000u; }  // NOLINT(readability-identifier-naming)

  void draw_absolute_pixel_internal(int x, int y, Color color) override;
};  // class

}  // namespace bb_epaper
}  // namespace esphome
