#pragma once

#include "esphome/components/display/display.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <array>

namespace esphome {
namespace inkplate {

enum InkplateModel : uint8_t {
  INKPLATE_6 = 0,
  INKPLATE_10 = 1,
  INKPLATE_6_PLUS = 2,
  INKPLATE_6_V2 = 3,
  INKPLATE_5 = 4,
  INKPLATE_5_V2 = 5,
};

static constexpr uint8_t NONE = 0;
static constexpr uint8_t MIRROR_X = 1;
static constexpr uint8_t MIRROR_Y = 2;
static constexpr uint8_t SWAP_XY = 4;

static constexpr uint8_t GLUT_SIZE = 9;
static constexpr uint8_t GLUT_COUNT = 8;

static constexpr uint8_t LUT2[16] = {0xAA, 0xA9, 0xA6, 0xA5, 0x9A, 0x99, 0x96, 0x95,
                                     0x6A, 0x69, 0x66, 0x65, 0x5A, 0x59, 0x56, 0x55};
static constexpr uint8_t LUTW[16] = {0xFF, 0xFE, 0xFB, 0xFA, 0xEF, 0xEE, 0xEB, 0xEA,
                                     0xBF, 0xBE, 0xBB, 0xBA, 0xAF, 0xAE, 0xAB, 0xAA};
static constexpr uint8_t LUTB[16] = {0xFF, 0xFD, 0xF7, 0xF5, 0xDF, 0xDD, 0xD7, 0xD5,
                                     0x7F, 0x7D, 0x77, 0x75, 0x5F, 0x5D, 0x57, 0x55};

static constexpr uint8_t PIXEL_MASK_LUT[8] = {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};
static constexpr uint8_t PIXEL_MASK_GLUT[2] = {0x0F, 0xF0};

class Inkplate : public display::Display, public i2c::I2CDevice {
 public:
  Inkplate(InkplateModel model, uint16_t width, uint16_t height, const std::array<InternalGPIOPin *, 8> &data_pins,
           GPIOPin *ckv, GPIOPin *gpio0_enable, GPIOPin *gmod, GPIOPin *oe, GPIOPin *powerup, GPIOPin *sph,
           GPIOPin *spv, GPIOPin *vcom, GPIOPin *wakeup, InternalGPIOPin *cl, InternalGPIOPin *le)
      : model_(model),
        width_(width),
        height_(height),
        display_data_pins_(data_pins),
        ckv_pin_(ckv),
        gpio0_enable_pin_(gpio0_enable),
        gmod_pin_(gmod),
        oe_pin_(oe),
        powerup_pin_(powerup),
        sph_pin_(sph),
        spv_pin_(spv),
        vcom_pin_(vcom),
        wakeup_pin_(wakeup),
        cl_pin_(cl),
        le_pin_(le) {
    // Compute data pin mask once
    this->data_pin_mask_ = 0;
    for (auto *pin : this->display_data_pins_) {
      this->data_pin_mask_ |= (1 << pin->get_pin());
    }
  }

  void set_greyscale(bool greyscale) {
    this->greyscale_ = greyscale;
    this->block_partial_ = true;
    if (this->is_ready())
      this->initialize_();
  }

  void set_waveform(const std::array<uint8_t, GLUT_COUNT * GLUT_SIZE> &waveform, bool is_custom) {
    static_assert(sizeof(this->waveform_) == sizeof(uint8_t) * GLUT_COUNT * GLUT_SIZE,
                  "waveform_ buffer size must match input waveform array size");
    memmove(this->waveform_, waveform.data(), sizeof(this->waveform_));
    this->custom_waveform_ = is_custom;
  }

  void set_partial_updating(bool partial_updating) { this->partial_updating_ = partial_updating; }
  void set_full_update_every(uint32_t full_update_every) { this->full_update_every_ = full_update_every; }
  void set_transform(uint8_t transform) { this->transform_ = transform; }

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
  void draw_pixel_at(int x, int y, Color color) override;
  bool rotate_coordinates_(int &x, int &y) const;
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

  void pins_z_state_();
  void pins_as_outputs_();

  int get_width_internal() override { return this->width_; }

  int get_height_internal() override { return this->height_; }

  int get_width() override { return this->transform_ & SWAP_XY ? this->height_ : this->width_; }
  int get_height() override { return this->transform_ & SWAP_XY ? this->width_ : this->height_; }

  size_t get_buffer_length_();

  int get_data_pin_mask_() { return this->data_pin_mask_; }

  bool panel_on_{false};
  uint8_t temperature_{};

  uint8_t *partial_buffer_{nullptr};
  uint8_t *partial_buffer_2_{nullptr};

  uint32_t *glut_{nullptr};
  uint32_t *glut2_{nullptr};
  uint32_t pin_lut_[256]{};

  unsigned full_update_every_{1};
  unsigned partial_updates_{0};

  bool block_partial_{true};
  bool greyscale_{};
  uint8_t transform_{false};
  bool partial_updating_{};
  bool custom_waveform_{false};
  uint8_t waveform_[GLUT_COUNT][GLUT_SIZE]{};
  uint8_t *buffer_{nullptr};

  InkplateModel model_;
  uint16_t width_;
  uint16_t height_;

  std::array<InternalGPIOPin *, 8> display_data_pins_;

  GPIOPin *ckv_pin_;
  GPIOPin *gpio0_enable_pin_;
  GPIOPin *gmod_pin_;
  GPIOPin *oe_pin_;
  GPIOPin *powerup_pin_;
  GPIOPin *sph_pin_;
  GPIOPin *spv_pin_;
  GPIOPin *vcom_pin_;
  GPIOPin *wakeup_pin_;
  InternalGPIOPin *cl_pin_;
  InternalGPIOPin *le_pin_;
  int data_pin_mask_;
};

}  // namespace inkplate
}  // namespace esphome
