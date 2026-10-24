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

// Before the first image is loaded, a widget gets a descriptor it can draw too
TEST(RuntimeImageLvgl, UnloadedImageHasADrawableDescriptor) {
  lv_init();
  RuntimeImage img(BMP, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
  lv_image_header_t header;
  ASSERT_EQ(lv_image_decoder_get_info(img.get_lv_image_dsc(), &header), LV_RESULT_OK);
  EXPECT_EQ(header.w, 1);
  EXPECT_EQ(header.h, 1);
  lv_deinit();
}

// With a placeholder, a widget shows it before the first image and while the next one loads
TEST(RuntimeImageLvgl, PlaceholderIsShownWithoutAnImage) {
  lv_init();
  static constexpr uint8_t PLACEHOLDER_PIXELS[4 * 2 * 3] = {};
  image::Image placeholder(PLACEHOLDER_PIXELS, 4, 2, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
  RuntimeImage img(BMP, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE, &placeholder);
  lv_image_dsc_t *dsc = img.get_lv_image_dsc();
  EXPECT_EQ(dsc->data, PLACEHOLDER_PIXELS);
  lv_image_header_t header;
  ASSERT_EQ(lv_image_decoder_get_info(dsc, &header), LV_RESULT_OK);
  EXPECT_EQ(header.w, 4);
  EXPECT_EQ(header.h, 2);

  std::vector<uint8_t> data(BMP_24BPP, BMP_24BPP + sizeof(BMP_24BPP));
  ASSERT_TRUE(img.begin_decode(data.size()));
  ASSERT_EQ(img.feed_data(data.data(), data.size()), static_cast<int>(data.size()));
  ASSERT_TRUE(img.end_decode());
  EXPECT_EQ(img.get_lv_image_dsc(), dsc);
  EXPECT_EQ(dsc->data, img.get_data_start());
  EXPECT_EQ(dsc->header.w, 3);

  img.release();
  EXPECT_EQ(dsc->data, PLACEHOLDER_PIXELS);
  EXPECT_EQ(dsc->header.w, 4);
  EXPECT_EQ(dsc->header.h, 2);
  lv_deinit();
}

#ifdef USE_LVGL_IMAGE
static constexpr int32_t SCREEN_WIDTH = 16;
static constexpr int32_t SCREEN_HEIGHT = 8;

static void flush_ready(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
  lv_display_flush_ready(display);
}

// A display to draw on, so ASan sees any read LVGL makes past an image's pixels
class RuntimeImageLvglWidget : public ::testing::Test {
 protected:
  void SetUp() override {
    lv_init();
    this->display_ = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(this->display_, flush_ready);
    lv_display_set_buffers(this->display_, this->draw_buffer_.data(), nullptr, this->draw_buffer_.size(),
                           LV_DISPLAY_RENDER_MODE_FULL);
    this->widget_ = lv_image_create(lv_screen_active());
  }
  void TearDown() override { lv_deinit(); }

  // What the lvgl component's lv_image_set_src() overload does
  void set_src(image::Image *image) {
    ::lv_image_set_src(this->widget_, image->get_lv_image_dsc());
    image->add_lvgl_widget(this->widget_);
  }

  static void load(RuntimeImage &img) {
    std::vector<uint8_t> data(BMP_24BPP, BMP_24BPP + sizeof(BMP_24BPP));
    ASSERT_TRUE(img.begin_decode(data.size()));
    ASSERT_EQ(img.feed_data(data.data(), data.size()), static_cast<int>(data.size()));
    ASSERT_TRUE(img.end_decode());
  }

  // Redraw the widget, as something drawn over it would make LVGL do during a load
  void draw() {
    lv_obj_invalidate(this->widget_);
    lv_refr_now(this->display_);
  }

  lv_display_t *display_{};
  lv_obj_t *widget_{};
  std::vector<uint8_t> draw_buffer_ = std::vector<uint8_t>(SCREEN_WIDTH * SCREEN_HEIGHT * 4);
};

// The widget keeps the size it read when given the descriptor, and draws that much of whatever the descriptor holds.
// The image gives the widget its source again whenever what it shows changes size.
TEST_F(RuntimeImageLvglWidget, WidgetFollowsTheImageSize) {
  RuntimeImage img(BMP, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
  this->set_src(&img);
  EXPECT_EQ(lv_image_get_src_width(this->widget_), 1);

  load(img);
  EXPECT_EQ(lv_image_get_src_width(this->widget_), 3);
  EXPECT_EQ(lv_image_get_src_height(this->widget_), 2);
  this->draw();

  img.release();
  EXPECT_EQ(lv_image_get_src_width(this->widget_), 1);
  EXPECT_EQ(lv_image_get_src_height(this->widget_), 1);
  this->draw();

  load(img);
  EXPECT_EQ(lv_image_get_src_width(this->widget_), 3);
  this->draw();
}

// The placeholder is smaller than the image, and on the heap, so drawing it at the image's size trips ASan
TEST_F(RuntimeImageLvglWidget, WidgetFollowsThePlaceholderSize) {
  std::vector<uint8_t> placeholder_pixels(2 * 1 * 3);
  image::Image placeholder(placeholder_pixels.data(), 2, 1, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
  RuntimeImage img(BMP, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE, &placeholder);
  load(img);
  this->set_src(&img);
  this->draw();

  img.release();
  EXPECT_EQ(lv_image_get_src_width(this->widget_), 2);
  EXPECT_EQ(lv_image_get_src_height(this->widget_), 1);
  this->draw();
}

// A widget given another image is left alone, however many times it was given this one
TEST_F(RuntimeImageLvglWidget, WidgetGivenAnotherImageIsLeftAlone) {
  static constexpr uint8_t OTHER_PIXELS[4 * 5 * 3] = {};
  image::Image other(OTHER_PIXELS, 4, 5, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
  RuntimeImage img(BMP, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
  load(img);
  this->set_src(&img);
  this->set_src(&img);
  this->set_src(&other);

  img.release();
  EXPECT_EQ(lv_image_get_src(this->widget_), other.get_lv_image_dsc());
  EXPECT_EQ(lv_image_get_src_width(this->widget_), 4);

  // Given back, it is followed again
  this->set_src(&img);
  load(img);
  EXPECT_EQ(lv_image_get_src_width(this->widget_), 3);
}

TEST_F(RuntimeImageLvglWidget, DeletedWidgetIsForgotten) {
  RuntimeImage img(BMP, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
  load(img);
  this->set_src(&img);
  lv_obj_delete(this->widget_);
  img.release();
}
#endif  // USE_LVGL_IMAGE

}  // namespace esphome::runtime_image::testing

#endif  // USE_LVGL
