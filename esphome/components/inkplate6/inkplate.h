#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/display/display_buffer.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

namespace esphome {
namespace inkplate6 {

enum InkplateModel : uint8_t {
  INKPLATE_6 = 0,
  INKPLATE_10 = 1,
  INKPLATE_6_PLUS = 2,
};

class Inkplate6 : public PollingComponent, public display::DisplayBuffer, public i2c::I2CDevice {
 public:
  const uint8_t LUT2[16] = {0xAA, 0xA9, 0xA6, 0xA5, 0x9A, 0x99, 0x96, 0x95,
                            0x6A, 0x69, 0x66, 0x65, 0x5A, 0x59, 0x56, 0x55};
  const uint8_t LUTW[16] = {0xFF, 0xFE, 0xFB, 0xFA, 0xEF, 0xEE, 0xEB, 0xEA,
                            0xBF, 0xBE, 0xBB, 0xBA, 0xAF, 0xAE, 0xAB, 0xAA};
  const uint8_t LUTB[16] = {0xFF, 0xFD, 0xF7, 0xF5, 0xDF, 0xDD, 0xD7, 0xD5,
                            0x7F, 0x7D, 0x77, 0x75, 0x5F, 0x5D, 0x57, 0x55};

  const uint8_t pixelMaskLUT[8] = {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};
  const uint8_t pixelMaskGLUT[2] = {0x0F, 0xF0};

  const uint8_t waveform3Bit[8][8] = {{0, 1, 1, 0, 0, 1, 1, 0}, {0, 1, 2, 1, 1, 2, 1, 0}, {1, 1, 1, 2, 2, 1, 0, 0},
                                      {0, 0, 0, 1, 1, 1, 2, 0}, {2, 1, 1, 1, 2, 1, 2, 0}, {2, 2, 1, 1, 2, 1, 2, 0},
                                      {1, 1, 1, 2, 1, 2, 2, 0}, {0, 0, 0, 0, 0, 0, 2, 0}};
  const uint8_t waveform3Bit6Plus[8][9] = {{0, 0, 0, 0, 0, 2, 1, 1, 0}, {0, 0, 2, 1, 1, 1, 2, 1, 0},
                                           {0, 2, 2, 2, 1, 1, 2, 1, 0}, {0, 0, 2, 2, 2, 1, 2, 1, 0},
                                           {0, 0, 0, 0, 2, 2, 2, 1, 0}, {0, 0, 2, 1, 2, 1, 1, 2, 0},
                                           {0, 0, 2, 2, 2, 1, 1, 2, 0}, {0, 0, 0, 0, 2, 2, 2, 2, 0}};

  void set_greyscale(bool greyscale) {
    this->greyscale_ = greyscale;
    this->initialize_();
    this->block_partial_ = true;
  }
  void set_partial_updating(bool partial_updating) { this->partial_updating_ = partial_updating; }
  void set_full_update_every(uint32_t full_update_every) { this->full_update_every_ = full_update_every; }

  void set_model(InkplateModel model) { this->model_ = model; }

  void set_display_data_0_pin(InternalGPIOPin *data) { this->display_data_0_pin_ = data; }
  void set_display_data_1_pin(InternalGPIOPin *data) { this->display_data_1_pin_ = data; }
  void set_display_data_2_pin(InternalGPIOPin *data) { this->display_data_2_pin_ = data; }
  void set_display_data_3_pin(InternalGPIOPin *data) { this->display_data_3_pin_ = data; }
  void set_display_data_4_pin(InternalGPIOPin *data) { this->display_data_4_pin_ = data; }
  void set_display_data_5_pin(InternalGPIOPin *data) { this->display_data_5_pin_ = data; }
  void set_display_data_6_pin(InternalGPIOPin *data) { this->display_data_6_pin_ = data; }
  void set_display_data_7_pin(InternalGPIOPin *data) { this->display_data_7_pin_ = data; }

  void set_ckv_pin(GPIOPin *ckv) { this->ckv_pin_ = ckv; }
  void set_cl_pin(InternalGPIOPin *cl) { this->cl_pin_ = cl; }
  void set_gpio0_enable_pin(GPIOPin *gpio0_enable) { this->gpio0_enable_pin_ = gpio0_enable; }
  void set_gmod_pin(GPIOPin *gmod) { this->gmod_pin_ = gmod; }
  void set_le_pin(InternalGPIOPin *le) { this->le_pin_ = le; }
  void set_oe_pin(GPIOPin *oe) { this->oe_pin_ = oe; }
  void set_powerup_pin(GPIOPin *powerup) { this->powerup_pin_ = powerup; }
  void set_sph_pin(GPIOPin *sph) { this->sph_pin_ = sph; }
  void set_spv_pin(GPIOPin *spv) { this->spv_pin_ = spv; }
  void set_vcom_pin(GPIOPin *vcom) { this->vcom_pin_ = vcom; }
  void set_wakeup_pin(GPIOPin *wakeup) { this->wakeup_pin_ = wakeup; }

  float get_setup_priority() const override;

  void dump_config() override;

  void display();
  void clean();
  void fill(Color color) override;

  void update() override;

  void setup() override;

  uint8_t get_panel_state() { return this->panel_on_; }
  bool get_greyscale() { return this->greyscale_; }
  bool get_partial_updating() { return this->partial_updating_; }
  uint8_t get_temperature() { return this->temperature_; }

  void block_partial() { this->block_partial_ = true; }

  display::DisplayType get_display_type() override {
    return get_greyscale() ? display::DisplayType::DISPLAY_TYPE_GRAYSCALE : display::DisplayType::DISPLAY_TYPE_BINARY;
  }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void display1b_();
  void display3b_();
  void initialize_();
  bool partial_update_();
  void clean_fast_(uint8_t c, uint8_t rep);

  void hscan_start_(uint32_t d);
  void vscan_end_();
  void vscan_start_();

  void eink_off_();
  void eink_on_();
  bool read_power_status_();

  void setup_pins_();
  void pins_z_state_();
  void pins_as_outputs_();

  int get_width_internal() override {
    if (this->model_ == INKPLATE_6) {
      return 800;
    } else if (this->model_ == INKPLATE_10) {
      return 1200;
    } else if (this->model_ == INKPLATE_6_PLUS) {
      return 1024;
    }
    return 0;
  }

  int get_height_internal() override {
    if (this->model_ == INKPLATE_6) {
      return 600;
    } else if (this->model_ == INKPLATE_10) {
      return 825;
    } else if (this->model_ == INKPLATE_6_PLUS) {
      return 758;
    }
    return 0;
  }

  size_t get_buffer_length_();

  int get_data_pin_mask_() {
    int data = 0;
    data |= (1 << this->display_data_0_pin_->get_pin());
    data |= (1 << this->display_data_1_pin_->get_pin());
    data |= (1 << this->display_data_2_pin_->get_pin());
    data |= (1 << this->display_data_3_pin_->get_pin());
    data |= (1 << this->display_data_4_pin_->get_pin());
    data |= (1 << this->display_data_5_pin_->get_pin());
    data |= (1 << this->display_data_6_pin_->get_pin());
    data |= (1 << this->display_data_7_pin_->get_pin());
    return data;
  }

  bool panel_on_{false};
  uint8_t temperature_;

  uint8_t *partial_buffer_{nullptr};
  uint8_t *partial_buffer_2_{nullptr};

  uint32_t *glut_{nullptr};
  uint32_t *glut2_{nullptr};
  uint32_t pin_lut_[256];

  uint32_t full_update_every_;
  uint32_t partial_updates_{0};

  bool block_partial_{true};
  bool greyscale_;
  bool partial_updating_;

  InkplateModel model_;

  InternalGPIOPin *display_data_0_pin_;
  InternalGPIOPin *display_data_1_pin_;
  InternalGPIOPin *display_data_2_pin_;
  InternalGPIOPin *display_data_3_pin_;
  InternalGPIOPin *display_data_4_pin_;
  InternalGPIOPin *display_data_5_pin_;
  InternalGPIOPin *display_data_6_pin_;
  InternalGPIOPin *display_data_7_pin_;

  GPIOPin *ckv_pin_;
  InternalGPIOPin *cl_pin_;
  GPIOPin *gpio0_enable_pin_;
  GPIOPin *gmod_pin_;
  InternalGPIOPin *le_pin_;
  GPIOPin *oe_pin_;
  GPIOPin *powerup_pin_;
  GPIOPin *sph_pin_;
  GPIOPin *spv_pin_;
  GPIOPin *vcom_pin_;
  GPIOPin *wakeup_pin_;
};

}  // namespace inkplate6
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
