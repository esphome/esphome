#pragma once
#include "image_decoder.h"
#include "runtime_image.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#ifdef USE_RUNTIME_IMAGE_JPEG_DEC
#include <JPEGDEC.h>

namespace esphome::runtime_image {

/**
 * @brief Image decoder specialization for JPEG images.
 */
class JpegDecoder : public ImageDecoder {
 public:
  /**
   * @brief Construct a new JPEG Decoder object.
   *
   * @param image The RuntimeImage to decode the stream into.
   */
  JpegDecoder(RuntimeImage *image);
  ~JpegDecoder() override;

  int prepare(size_t expected_size) override;
  int HOT decode(uint8_t *buffer, size_t size) override;
  void reset() override;

  /// The source is read as the decode needs it, through JPEGDEC's own 2 KB buffer
  bool supports_source() const override { return true; }
  int begin_source(DecodeSource *source) override;
  /// Decodes one row of MCUs (8 or 16 pixel rows), or waits part way through one for more of the source
  int HOT decode_step() override;

  /// JPEGDEC decodes the image at 1/get_scale() of its size: 1, 2, 4 or 8
  int get_scale() const { return this->scale_; }
  void draw_rgb565(int x, int y, int w, int h, const uint8_t *pixels);

 protected:
  /// Set up an opened image for decoding. Returns 0, or a DecodeError.
  int start_();
  /// The JPEGDEC option for scale_: JPEG_SCALE_HALF, _QUARTER and _EIGHTH are the divisors themselves
  int scale_option_() const { return this->scale_ == 1 ? 0 : this->scale_; }
  /// The DecodeError for a failed read of the source or JPEGDEC's last error, closing the image
  int fail_();

  static int32_t source_read(JPEGFILE *file, uint8_t *buf, int32_t len);
  static int32_t source_seek(JPEGFILE *file, int32_t position);
  static void source_close(void *handle) {}

  /// About 18 KB of decoder state, allocated like pngle in PngDecoder: PSRAM first, nullptr if out of memory
  JPEGDEC *jpeg_{nullptr};
  int scale_{1};                   // JPEGDEC decodes at 1/scale_ of the image size
  DecodeSource *source_{nullptr};  // Set while a begin_source() decode is in progress
  bool source_failed_{false};
};

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_JPEG_DEC
