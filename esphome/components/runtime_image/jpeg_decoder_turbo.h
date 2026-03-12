#pragma once

#include "image_decoder.h"
#include "runtime_image.h"
#include "esphome/core/defines.h"
#ifdef USE_RUNTIME_IMAGE_JPEG_TURBO
#include <jpeglib.h>
#include <csetjmp>

namespace esphome::runtime_image {

struct JpegErrorMgr {
  jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
  char message[JMSG_LENGTH_MAX];
};

/**
 * @brief JPEG decoder using libjpeg-turbo.
 *
 * Uses IDCT-based downscaling (1/8, 1/4, 1/2, 1/1) to reduce decode
 * work when fixed target dimensions are configured. Processes scanlines
 * in chunks to avoid starving the main loop.
 */
class JpegDecoder : public ImageDecoder {
 public:
  JpegDecoder(RuntimeImage *image) : ImageDecoder(image) {}
  ~JpegDecoder() override { this->cleanup_(); }

  int prepare(size_t expected_size) override;
  int HOT decode(uint8_t *buffer, size_t size) override;
  bool is_finished() const override { return this->phase_ == FINISHED; }

 private:
  enum Phase { WAITING, DECOMPRESSING, FINISHED };

  void cleanup_();

  Phase phase_{WAITING};
  jpeg_decompress_struct *cinfo_{nullptr};
  JpegErrorMgr *jerr_{nullptr};
  uint8_t *row_buffer_{nullptr};
  int out_w_{0};
  int current_scanline_{0};

  static constexpr int SCANLINES_PER_CHUNK = 100;
};

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_JPEG_TURBO
