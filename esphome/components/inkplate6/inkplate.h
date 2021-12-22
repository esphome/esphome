#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/display/display_buffer.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

namespace esphome {
namespace inkplate6 {

class Inkplate6 : public PollingComponent, public display::DisplayBuffer, public i2c::I2CDevice {
 public:
  const uint8_t LUT2[16] = {0b10101010, 0b10101001, 0b10100110, 0b10100101, 0b10011010, 0b10011001,
                            0b10010110, 0b10010101, 0b01101010, 0b01101001, 0b01100110, 0b01100101,
                            0b01011010, 0b01011001, 0b01010110, 0b01010101};
  const uint8_t LUTW[16] = {0b11111111, 0b11111110, 0b11111011, 0b11111010, 0b11101111, 0b11101110,
                            0b11101011, 0b11101010, 0b10111111, 0b10111110, 0b10111011, 0b10111010,
                            0b10101111, 0b10101110, 0b10101011, 0b10101010};
  const uint8_t LUTB[16] = {0b11111111, 0b11111101, 0b11110111, 0b11110101, 0b11011111, 0b11011101,
                            0b11010111, 0b11010101, 0b01111111, 0b01111101, 0b01110111, 0b01110101,
                            0b01011111, 0b01011101, 0b01010111, 0b01010101};
  const uint8_t pixelMaskLUT[8] = {0b00000001, 0b00000010, 0b00000100, 0b00001000,
                                   0b00010000, 0b00100000, 0b01000000, 0b10000000};
  const uint8_t pixelMaskGLUT[2] = {0b00001111, 0b11110000};
  const uint8_t waveform3Bit[8][8] = {{0, 0, 0, 0, 1, 1, 1, 0}, {1, 2, 2, 2, 1, 1, 1, 0}, {0, 1, 2, 1, 1, 2, 1, 0},
                                      {0, 2, 1, 2, 1, 2, 1, 0}, {0, 0, 0, 1, 1, 1, 2, 0}, {2, 1, 1, 1, 2, 1, 2, 0},
                                      {1, 1, 1, 2, 1, 2, 2, 0}, {0, 0, 0, 0, 0, 0, 2, 0}};
  const uint32_t waveform[50] = {
      0x00000008, 0x00000008, 0x00200408, 0x80281888, 0x60a81898, 0x60a8a8a8, 0x60a8a8a8, 0x6068a868, 0x6868a868,
      0x6868a868, 0x68686868, 0x6a686868, 0x5a686868, 0x5a686868, 0x5a586a68, 0x5a5a6a68, 0x5a5a6a68, 0x55566a68,
      0x55565a64, 0x55555654, 0x55555556, 0x55555556, 0x55555556, 0x55555516, 0x55555596, 0x15555595, 0x95955595,
      0x95959595, 0x95949495, 0x94949495, 0x94949495, 0xa4949494, 0x9494a4a4, 0x84a49494, 0x84948484, 0x84848484,
      0x84848484, 0x84848484, 0xa5a48484, 0xa9a4a4a8, 0xa9a8a8a8, 0xa5a9a9a4, 0xa5a5a5a4, 0xa1a5a5a1, 0xa9a9a9a9,
      0xa9a9a9a9, 0xa9a9a9a9, 0xa9a9a9a9, 0x15151515, 0x11111111};

  void set_greyscale(bool greyscale) {
    this->greyscale_ = greyscale;
    this->initialize_();
    this->block_partial_ = true;
  }
  void set_partial_updating(bool partial_updating) { this->partial_updating_ = partial_updating; }
  void set_full_update_every(uint32_t full_update_every) { this->full_update_every_ = full_update_every; }

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
  void vscan_write_();

  void eink_off_();
  void eink_on_();

  void setup_pins_();
  void pins_z_state_();
  void pins_as_outputs_();

  int get_width_internal() override { return 800; }

  int get_height_internal() override { return 600; }

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

  uint8_t panel_on_ = 0;
  uint8_t temperature_;

  uint8_t *partial_buffer_{nullptr};
  uint8_t *partial_buffer_2_{nullptr};

  uint32_t full_update_every_;
  uint32_t partial_updates_{0};

  bool block_partial_;
  bool greyscale_;
  bool partial_updating_;

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
