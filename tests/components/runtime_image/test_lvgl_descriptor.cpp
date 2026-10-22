#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/runtime_image/runtime_image.h"

#ifdef USE_LVGL

namespace esphome::runtime_image::testing {

// 3x2 24bpp BMP (rows padded to 4 bytes)
static const uint8_t BMP_24BPP[] = {
    0x42, 0x4D, 0x4E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
    0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x22, 0x11, 0x77, 0x88, 0x99, 0xEF, 0xCD, 0xAB, 0x00,
    0x00, 0x00, 0x20, 0x10, 0xE0, 0x40, 0xC0, 0x30, 0xA0, 0x60, 0x50, 0x00, 0x00, 0x00,
};

// A widget keeps the descriptor it was given and reads it on every draw. Once the image lets go of its pixels,
// for example when it starts loading another image, the descriptor must still be one LVGL can draw.
TEST(RuntimeImageLvgl, ReleasedImageLeavesADrawableDescriptor) {
  lv_init();
  RuntimeImage img(BMP, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
  std::vector<uint8_t> data(BMP_24BPP, BMP_24BPP + sizeof(BMP_24BPP));
  ASSERT_TRUE(img.begin_decode(data.size()));
  ASSERT_EQ(img.feed_data(data.data(), data.size()), static_cast<int>(data.size()));
  ASSERT_TRUE(img.end_decode());

  lv_image_dsc_t *dsc = img.get_lv_image_dsc();
  lv_image_header_t header;
  ASSERT_EQ(lv_image_decoder_get_info(dsc, &header), LV_RESULT_OK);
  EXPECT_EQ(header.w, 3);
  EXPECT_EQ(header.h, 2);

  img.release();
  EXPECT_EQ(lv_image_decoder_get_info(dsc, &header), LV_RESULT_OK);
  EXPECT_EQ(header.w, 1);
  EXPECT_EQ(header.h, 1);
  lv_deinit();
}

}  // namespace esphome::runtime_image::testing

#endif  // USE_LVGL
