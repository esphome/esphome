#include "esphome/core/log.h"
#include "inkplate4.h"

#ifdef USE_ESP32

#include "esp_rom_sys.h"

namespace esphome::inkplate {

static const char *const TAG = "inkplate4";

// Source: Inkplate4Driver display1b() / display3b() — same 5-step sequence for both paths.
const Inkplate4::CleanStep Inkplate4::CLEAN_SEQ[5] = {
    {0, 5}, {1, 15}, {0, 15}, {1, 15}, {0, 15},
};

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------

void Inkplate4::setup() {
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
      uint8_t v = (uint8_t) (((uint32_t) INKPLATE4_WAVEFORM3BIT[i & 0x07][j] << 2u) |
                             (uint32_t) INKPLATE4_WAVEFORM3BIT[(i >> 4) & 0x07][j]);
      this->glut_[j * 256 + i] = v;
      this->glut2_[j * 256 + i] = (uint8_t) (v << 4u);
    }
  }

  this->i2s_init_();
  this->tps_begin_();

  ESP_LOGI(TAG, "Inkplate4 setup done — %dx%d", this->width_, this->height_);
}

void Inkplate4::dump_config() {
  ESP_LOGCONFIG(TAG, "Inkplate 4 %dx%d, dark_phases=%d, partial_phases=%d, grayscale_phases=%d", this->width_,
                this->height_, this->dark_phases_, this->partial_phases_, this->grayscale_phases_);
}

// ---------------------------------------------------------------------------
// do_board_transfer_step
//
// Overrides all standard cases because width=600 is not 16-pixel aligned:
//   - TRF_DARK / TRF_LUT2 / TRF_PARTIAL_SEND / TRF_GRAYSCALE_SEND: inner loop
//     bound is (width_/16)*4; a remainder block handles the last 8 pixels.
//   - TRF_ZERO transitions to TRF_FINAL_SKIP (not TRF_FINAL_VSCAN).
//   - TRF_FINAL_SKIP: extra clean(3,1) = 0xFF pass absent on other boards.
// TRF_PARTIAL_CLEAN_SKIP is standard — falls through to base.
// ---------------------------------------------------------------------------

bool Inkplate4::do_board_transfer_step() {
  switch (this->trf_sub_) {
    case TRF_DARK: {
      const uint8_t *dmp = this->d_memory_new_ + (size_t) this->width_ * this->height_ / 8 - 1;
      vscan_start_();
      for (int i = 0; i < this->height_; i++) {
        for (int n = 0; n < (this->width_ / 16) * 4; n += 4) {
          uint8_t dram1 = *dmp;
          uint8_t dram2 = *(dmp - 1);
          this->dma_line_buf_[n] = INKPLATE_LUTB[(dram2 >> 4) & 0x0F];
          this->dma_line_buf_[n + 1] = INKPLATE_LUTB[dram2 & 0x0F];
          this->dma_line_buf_[n + 2] = INKPLATE_LUTB[(dram1 >> 4) & 0x0F];
          this->dma_line_buf_[n + 3] = INKPLATE_LUTB[dram1 & 0x0F];
          dmp -= 2;
        }
        // Remainder: 1 source byte → DMA[width_/4] and DMA[width_/4+1] (EPD px 592–599).
        {
          uint8_t drem = *dmp--;
          this->dma_line_buf_[this->width_ / 4] = INKPLATE_LUTB[(drem >> 4) & 0x0F];
          this->dma_line_buf_[this->width_ / 4 + 1] = INKPLATE_LUTB[drem & 0x0F];
        }
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_k_++;
      if (this->trf_k_ >= this->dark_phases_)
        this->trf_sub_ = TRF_LUT2;
      return false;
    }

    case TRF_LUT2: {
      const uint8_t *dmp = this->d_memory_new_ + (size_t) this->width_ * this->height_ / 8 - 1;
      vscan_start_();
      for (int i = 0; i < this->height_; i++) {
        for (int n = 0; n < (this->width_ / 16) * 4; n += 4) {
          uint8_t dram1 = *dmp;
          uint8_t dram2 = *(dmp - 1);
          this->dma_line_buf_[n] = INKPLATE_LUT2[(dram2 >> 4) & 0x0F];
          this->dma_line_buf_[n + 1] = INKPLATE_LUT2[dram2 & 0x0F];
          this->dma_line_buf_[n + 2] = INKPLATE_LUT2[(dram1 >> 4) & 0x0F];
          this->dma_line_buf_[n + 3] = INKPLATE_LUT2[dram1 & 0x0F];
          dmp -= 2;
        }
        {
          uint8_t drem = *dmp--;
          this->dma_line_buf_[this->width_ / 4] = INKPLATE_LUT2[(drem >> 4) & 0x0F];
          this->dma_line_buf_[this->width_ / 4 + 1] = INKPLATE_LUT2[drem & 0x0F];
        }
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_sub_ = TRF_ZERO;
      return false;
    }

    // Discharge pass; Inkplate4 adds TRF_FINAL_SKIP after this.
    case TRF_ZERO: {
      vscan_start_();
      memset((void *) this->dma_line_buf_, 0, (size_t) this->width_ / 4 + 16);
      for (int i = 0; i < this->height_; i++) {
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_sub_ = TRF_FINAL_SKIP;
      return false;
    }

    // clean(3,1) = 0xFF skip pass; not present on other boards.
    case TRF_FINAL_SKIP: {
      vscan_start_();
      memset((void *) this->dma_line_buf_, 0xFF, (size_t) this->width_ / 4 + 16);
      for (int i = 0; i < this->height_; i++) {
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_sub_ = TRF_FINAL_VSCAN;
      return false;
    }

    case TRF_PARTIAL_SEND: {
      const uint8_t *pb = this->p_buffer_ + (size_t) this->width_ * this->height_ / 4 - 1;
      vscan_start_();
      for (int i = 0; i < this->height_; i++) {
        for (int j = 0; j < (this->width_ / 16) * 4; j += 4) {
          this->dma_line_buf_[j + 2] = *pb--;
          this->dma_line_buf_[j + 3] = *pb--;
          this->dma_line_buf_[j] = *pb--;
          this->dma_line_buf_[j + 1] = *pb--;
        }
        // Remainder: 2 source reads → DMA[width_/4] and DMA[width_/4+1].
        this->dma_line_buf_[this->width_ / 4] = *pb--;
        this->dma_line_buf_[this->width_ / 4 + 1] = *pb--;
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_k_++;
      if (this->trf_k_ >= this->partial_phases_) {
        this->trf_sub_ = TRF_PARTIAL_CLEAN_DISC;
        this->trf_k_ = 0;
      }
      return false;
    }

    case TRF_GRAYSCALE_SEND: {
      const uint8_t *dp = this->d_memory_4bit_ + (size_t) this->width_ * this->height_ / 2;
      vscan_start_();
      for (int i = 0; i < this->height_; i++) {
        for (int j = 0; j < (this->width_ / 16) * 4; j += 4) {
          uint8_t pix_hi, pix_lo;
          pix_hi = *(--dp);
          pix_lo = *(--dp);
          this->dma_line_buf_[j + 2] =
              (uint8_t) (this->glut2_[this->trf_k_ * 256 + pix_hi] | this->glut_[this->trf_k_ * 256 + pix_lo]);
          pix_hi = *(--dp);
          pix_lo = *(--dp);
          this->dma_line_buf_[j + 3] =
              (uint8_t) (this->glut2_[this->trf_k_ * 256 + pix_hi] | this->glut_[this->trf_k_ * 256 + pix_lo]);
          pix_hi = *(--dp);
          pix_lo = *(--dp);
          this->dma_line_buf_[j] =
              (uint8_t) (this->glut2_[this->trf_k_ * 256 + pix_hi] | this->glut_[this->trf_k_ * 256 + pix_lo]);
          pix_hi = *(--dp);
          pix_lo = *(--dp);
          this->dma_line_buf_[j + 1] =
              (uint8_t) (this->glut2_[this->trf_k_ * 256 + pix_hi] | this->glut_[this->trf_k_ * 256 + pix_lo]);
        }
        // Remainder: 4 source reads → DMA[width_/4] and DMA[width_/4+1].
        uint8_t pix_hi, pix_lo;
        pix_hi = *(--dp);
        pix_lo = *(--dp);
        this->dma_line_buf_[this->width_ / 4] =
            (uint8_t) (this->glut2_[this->trf_k_ * 256 + pix_hi] | this->glut_[this->trf_k_ * 256 + pix_lo]);
        pix_hi = *(--dp);
        pix_lo = *(--dp);
        this->dma_line_buf_[this->width_ / 4 + 1] =
            (uint8_t) (this->glut2_[this->trf_k_ * 256 + pix_hi] | this->glut_[this->trf_k_ * 256 + pix_lo]);
        send_line_i2s_();
        vscan_end_();
      }
      esp_rom_delay_us(230);
      this->trf_k_++;
      if (this->trf_k_ >= this->grayscale_phases_) {
        this->trf_sub_ = TRF_GRAYSCALE_FINAL_CLEAN;
        this->trf_k_ = 0;
      }
      return false;
    }

    default:
      return InkplateParallelBase::do_board_transfer_step();
  }
}

}  // namespace esphome::inkplate

#endif  // USE_ESP32
