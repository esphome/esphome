#include "esphome/core/log.h"
#include "inkplate10.h"

#ifdef USE_ESP32

#include "esp_rom_sys.h"

namespace esphome::inkplate {

static const char *const TAG = "inkplate10";

const Inkplate10::CleanStep Inkplate10::CLEAN_SEQ_1B[8] = {
    {0, 1}, {1, 10}, {2, 1}, {0, 10}, {2, 1}, {1, 10}, {2, 1}, {0, 10},
};

const Inkplate10::CleanStep Inkplate10::CLEAN_SEQ_3B[8] = {
    {1, 1}, {0, 10}, {2, 1}, {1, 10}, {2, 1}, {0, 10}, {2, 1}, {1, 10},
};

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------

void Inkplate10::setup() {
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
      uint8_t v = (uint8_t) (((uint32_t) INKPLATE10_WAVEFORM3BIT[i & 0x07][j] << 2u) |
                             (uint32_t) INKPLATE10_WAVEFORM3BIT[(i >> 4) & 0x07][j]);
      this->glut_[j * 256 + i] = v;
      this->glut2_[j * 256 + i] = (uint8_t) (v << 4u);
    }
  }

  this->i2s_init_();
  this->tps_begin_();

  ESP_LOGI(TAG, "Inkplate10 setup done — %dx%d", this->width_, this->height_);
}

void Inkplate10::dump_config() {
  ESP_LOGCONFIG(TAG, "Inkplate 10 %dx%d, dark_phases=%d, partial_phases=%d, grayscale_phases=%d", this->width_,
                this->height_, this->dark_phases_, this->partial_phases_, this->grayscale_phases_);
}

// ---------------------------------------------------------------------------
// clean_data_byte — selects sequence based on update path.
// CLEAN_SEQ_1B and CLEAN_SEQ_3B have identical .rep values; clean_seq_ is
// set to CLEAN_SEQ_1B in the constructor for rep tracking. Only .c differs.
// ---------------------------------------------------------------------------

uint8_t Inkplate10::clean_data_byte() const {
  const CleanStep *seq = (this->trf_after_clean_ == TRF_GRAYSCALE_SEND) ? CLEAN_SEQ_3B : CLEAN_SEQ_1B;
  switch (seq[this->trf_step_].c) {
    case 0:
      return 0b10101010;
    case 1:
      return 0b01010101;
    case 2:
      return 0b00000000;
    case 3:
      return 0b11111111;
    default:
      return 0;
  }
}

}  // namespace esphome::inkplate

#endif  // USE_ESP32
