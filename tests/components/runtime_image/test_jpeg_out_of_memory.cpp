#include <gtest/gtest.h>

#include "esphome/components/runtime_image/jpeg_decoder.h"
#include "esphome/components/runtime_image/runtime_image.h"

#include <utility>

#ifdef USE_RUNTIME_IMAGE_JPEG

namespace esphome::runtime_image::testing {

// A decoder whose JPEGDEC state is missing, as when its allocation fails. The state moves to state_, which frees it.
class StatelessJpegDecoder : public JpegDecoder {
 public:
  explicit StatelessJpegDecoder(RuntimeImage *image) : JpegDecoder(image), state_(std::move(this->jpeg_)) {}

 protected:
  RAMUniquePtr<JPEGDEC> state_;
};

// The ~23 KB of JPEGDEC state lives in its own allocation, so it can come from PSRAM
TEST(JpegOutOfMemory, DecoderStateIsAllocatedSeparately) { EXPECT_LT(sizeof(JpegDecoder), sizeof(JPEGDEC) / 4); }

TEST(JpegOutOfMemory, MissingStateFailsWithoutStartingASession) {
  RuntimeImage img(JPEG, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE);
  StatelessJpegDecoder decoder(&img);
  EXPECT_EQ(decoder.prepare(100), DECODE_ERROR_OUT_OF_MEMORY);
  EXPECT_FALSE(decoder.is_active());
  uint8_t data[4] = {};
  EXPECT_EQ(decoder.decode(data, sizeof(data)), DECODE_ERROR_OUT_OF_MEMORY);
  EXPECT_EQ(decoder.begin_source(nullptr), DECODE_ERROR_OUT_OF_MEMORY);
  decoder.reset();
}

}  // namespace esphome::runtime_image::testing

#endif  // USE_RUNTIME_IMAGE_JPEG
