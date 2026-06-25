#include "esphome/core/log.h"
#include "inkplate6plus.h"

#ifdef USE_ESP32

#include "esp_rom_sys.h"

namespace esphome::inkplate {

static const char *const TAG = "inkplate6plus";

// Source: Inkplate6PLUSDriver.cpp EPDDriver::display1b() / display3b() — same 6-step sequence for both.
const Inkplate6PLUS::CleanStep Inkplate6PLUS::CLEAN_SEQ[6] = {
    {0, 1}, {1, 15}, {2, 1}, {0, 5}, {2, 1}, {1, 15},
};

void Inkplate6PLUS::setup() {
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
      uint8_t v = (uint8_t) (((uint32_t) INKPLATE6PLUS_WAVEFORM3BIT[i & 0x07][j] << 2u) |
                             (uint32_t) INKPLATE6PLUS_WAVEFORM3BIT[(i >> 4) & 0x07][j]);
      this->glut_[j * 256 + i] = v;
      this->glut2_[j * 256 + i] = (uint8_t) (v << 4u);
    }
  }

  this->i2s_init_();
  this->tps_begin_();

  ESP_LOGI(TAG, "Inkplate6PLUS setup done — %dx%d", this->width_, this->height_);
}

void Inkplate6PLUS::dump_config() {
  ESP_LOGCONFIG(TAG, "Inkplate 6PLUS %dx%d, dark_phases=%d, partial_phases=%d, grayscale_phases=%d", this->width_,
                this->height_, this->dark_phases_, this->partial_phases_, this->grayscale_phases_);
}

}  // namespace esphome::inkplate

#endif  // USE_ESP32
