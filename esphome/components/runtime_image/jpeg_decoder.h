#pragma once
#include "image_decoder.h"
#include "runtime_image.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#ifdef USE_RUNTIME_IMAGE_JPEG
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
  JpegDecoder(RuntimeImage *image) : ImageDecoder(image, JPEG) {}
  ~JpegDecoder() override {}

  int HOT decode(uint8_t *buffer, size_t size) override;
  void reset() override;

  /// The source is read as the decode needs it, through JPEGDEC's own 2 KB buffer
  bool supports_source() const override { return true; }
  int begin_source(DecodeSource *source) override;
  /// Decodes one row of MCUs (8 or 16 pixel rows), or waits part way through one for more of the source
  int HOT decode_step() override;

  bool uses_direct_rgb565() const { return this->direct_rgb565_; }
  void draw_rgb565(int x, int y, int w, int h, const uint8_t *pixels);

 protected:
  bool prepare_rgb565_scaling_(int width, int height);
  /// Set up an opened image for decoding. Returns 0, or a DecodeError.
  int start_();
  /// The DecodeError for a failed read of the source or JPEGDEC's last error, closing the image
  int fail_();

  static int32_t source_read(JPEGFILE *file, uint8_t *buf, int32_t len);
  static int32_t source_seek(JPEGFILE *file, int32_t position);
  static void source_close(void *handle) {}

  JPEGDEC jpeg_{};
  FixedVector<uint16_t> x_boundaries_{};
  FixedVector<uint16_t> y_boundaries_{};
  int source_width_{0};
  int source_height_{0};
  DecodeSource *source_{nullptr};  // Set while a begin_source() decode is in progress
  bool direct_rgb565_{false};
  bool source_failed_{false};
};

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_JPEG
