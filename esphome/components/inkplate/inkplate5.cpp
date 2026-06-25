#include "esphome/core/log.h"
#include "inkplate5.h"

#ifdef USE_ESP32

#include "esp_rom_sys.h"

namespace esphome::inkplate {

static const char *const TAG = "inkplate5v2";

const Inkplate5::CleanStep Inkplate5::CLEAN_SEQ[8] = {
    {0, 1}, {1, 11}, {2, 1}, {0, 11}, {2, 1}, {1, 11}, {2, 1}, {0, 11},
};

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------

void Inkplate5::setup() {
  InkplateParallelBase::setup();
  if (this->is_failed())
    return;

  RAMAllocator<uint8_t> allocator;
  this->glut_ = allocator.allocate(256 * this->grayscale_phases_);
  this->glut2_ = allocator.allocate(256 * this->grayscale_phases_);
  if (this->glut_ == nullptr || this->glut2_ == nullptr) {
    ESP_LOGE(TAG, "GLUT alloc failed");
    this->mark_failed();
    return;
  }
  for (int j = 0; j < this->grayscale_phases_; ++j) {
    for (int i = 0; i < 256; ++i) {
      uint8_t v = (uint8_t) (((uint32_t) INKPLATE5_WAVEFORM3BIT[i & 0x07][j] << 2u) |
                             (uint32_t) INKPLATE5_WAVEFORM3BIT[(i >> 4) & 0x07][j]);
      this->glut_[j * 256 + i] = v;
      this->glut2_[j * 256 + i] = (uint8_t) (v << 4u);
    }
  }

  this->i2s_init_();
  this->tps_begin_();

  ESP_LOGI(TAG, "Inkplate5 setup done — %dx%d", this->width_, this->height_);
}

void Inkplate5::dump_config() {
  ESP_LOGCONFIG(TAG, "Inkplate 5 %dx%d, dark_phases=%d, partial_phases=%d, grayscale_phases=%d", this->width_,
                this->height_, this->dark_phases_, this->partial_phases_, this->grayscale_phases_);
}

// ---------------------------------------------------------------------------
// do_board_transfer_step
//
// Only overrides TRF_PARTIAL_CLEAN_SKIP: Arduino Inkplate5 skips the 0xFF
// vscan pass after the discharge passes — buffer sync only.
// All other cases fall through to base.
// ---------------------------------------------------------------------------

bool Inkplate5::do_board_transfer_step() {
  switch (this->trf_sub_) {
    case TRF_PARTIAL_CLEAN_SKIP:
      memcpy(this->d_memory_new_, this->buffer_, (size_t) this->width_ * this->height_ / 8);
      this->trf_sub_ = TRF_FINAL_VSCAN;
      return false;

    default:
      return InkplateParallelBase::do_board_transfer_step();
  }
}

// ---------------------------------------------------------------------------
// Inkplate5V1
// ---------------------------------------------------------------------------

// Source: Inkplate5Driver.cpp (Arduino library) EPDDriver::display1b() / display3b()
// #ifdef ARDUINO_INKPLATE5 — 9 steps, rep=14
const Inkplate5V1::CleanStep Inkplate5V1::CLEAN_SEQ_V1[9] = {
    {0, 1}, {1, 14}, {2, 1}, {0, 14}, {2, 1}, {1, 14}, {2, 1}, {0, 14}, {2, 1},
};

void Inkplate5V1::setup() {
  InkplateParallelBase::setup();  // NOLINT(bugprone-parent-virtual-call)
  if (this->is_failed())
    return;

  RAMAllocator<uint8_t> allocator;
  this->glut_ = allocator.allocate(256 * this->grayscale_phases_);
  this->glut2_ = allocator.allocate(256 * this->grayscale_phases_);
  if (this->glut_ == nullptr || this->glut2_ == nullptr) {
    ESP_LOGE(TAG, "GLUT alloc failed");
    this->mark_failed();
    return;
  }
  for (int j = 0; j < this->grayscale_phases_; ++j) {
    for (int i = 0; i < 256; ++i) {
      uint8_t v = (uint8_t) (((uint32_t) INKPLATE5_V1_WAVEFORM3BIT[i & 0x07][j] << 2u) |
                             (uint32_t) INKPLATE5_V1_WAVEFORM3BIT[(i >> 4) & 0x07][j]);
      this->glut_[j * 256 + i] = v;
      this->glut2_[j * 256 + i] = (uint8_t) (v << 4u);
    }
  }

  this->i2s_init_();
  this->tps_begin_();

  ESP_LOGI(TAG, "Inkplate5V1 setup done — %dx%d", this->width_, this->height_);
}

void Inkplate5V1::dump_config() {
  ESP_LOGCONFIG(TAG, "Inkplate 5 V1 %dx%d, dark_phases=%d, partial_phases=%d, grayscale_phases=%d", this->width_,
                this->height_, this->dark_phases_, this->partial_phases_, this->grayscale_phases_);
}

}  // namespace esphome::inkplate

#endif  // USE_ESP32
