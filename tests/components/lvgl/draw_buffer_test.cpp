#include <gtest/gtest.h>
#include "esphome/components/display/display.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

namespace esphome::lvgl::testing {

namespace {

// 33 x 33 pixels of RGB565 is 2178 bytes, which is not a multiple of LV_DRAW_BUF_ALIGN
constexpr int DISPLAY_SIZE = 33;

class FakeDisplay : public display::Display {
 public:
  void draw_pixel_at(int x, int y, Color color) override {}
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  void update() override {}

 protected:
  int get_width_internal() override { return DISPLAY_SIZE; }
  int get_height_internal() override { return DISPLAY_SIZE; }
};

}  // namespace

// LVGL will round the size an area needs up to LV_DRAW_BUF_ALIGN. The buffer must still hold an area that fills
// the screen, or LVGL will draw that area with the shape of the previous one.
TEST(LvglDrawBuffer, HoldsFullScreenAreaWhenSizeIsNotAligned) {
  ASSERT_NE(DISPLAY_SIZE * DISPLAY_SIZE * (LV_COLOR_DEPTH / 8) % LV_DRAW_BUF_ALIGN, 0)
      << "the display size no longer tests the rounding";
  if (!lv_is_initialized())
    LvglComponent::esphome_lvgl_init();
  FakeDisplay display;
  LvglComponent lvgl({&display}, 1.0f, false, 1, false, false, ROTATION_UNUSED);
  lvgl.setup();
  ASSERT_FALSE(lvgl.is_failed());

  auto *draw_buf = lv_display_get_buf_active(lvgl.get_disp());
  ASSERT_NE(draw_buf, nullptr);
  EXPECT_NE(lv_draw_buf_reshape(draw_buf, LV_COLOR_FORMAT_UNKNOWN, DISPLAY_SIZE, DISPLAY_SIZE, LV_STRIDE_AUTO),
            nullptr);
}

}  // namespace esphome::lvgl::testing
