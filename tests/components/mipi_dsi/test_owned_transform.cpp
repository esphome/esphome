#if defined(USE_HOST) && defined(MIPI_TEST_IDF_MAJOR)
#include "esphome/components/mipi_dsi/mipi_dsi.h"
#include "esphome/core/hal.h"
#include "mipi_test_sdk.h"
#include <array>
#include <atomic>
#include <future>
#include <gtest/gtest.h>
#include <limits>
namespace esphome::mipi_dsi::testing {
struct OwnedRig {
  MipiDsi *driver;
  TestDpiPanel *panel;
};
OwnedRig new_owned() {
  auto *driver = new MipiDsi(8, 4, display::COLOR_BITNESS_565, 16);
  driver->set_auto_clear(false);
  driver->set_writer([](display::Display &) {});
  driver->setup();
  return {driver, mipi_test_sdk::last_panel};
}
ppa_srm_oper_config_t request(const uint16_t *pixels, unsigned width = 3, unsigned height = 2, float scale = 1) {
  ppa_srm_oper_config_t c{};
  c.in.buffer = pixels;
  c.in.pic_w = c.in.block_w = width;
  c.in.pic_h = c.in.block_h = height;
  c.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  c.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  c.scale_x = c.scale_y = scale;
  // Caller outputs cannot redirect the driver-owned destination.
  c.out.buffer = reinterpret_cast<void *>(1);
  c.out.buffer_size = 1;
  return c;
}
class MipiOwned : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { arch_init(); }
  void SetUp() override { mipi_test_sdk::reset_calls(); }
};
class MipiOwnedRotation : public MipiOwned, public ::testing::WithParamInterface<display::DisplayRotation> {};
TEST_P(MipiOwnedRotation, LogicalOnePassMatchesThePublicScalarDisplay) {
  auto actual = new_owned(), expected = new_owned();
  actual.driver->set_rotation(GetParam());
  expected.driver->set_rotation(GetParam());
  const uint16_t pixels[]{0xf800, 0x07e0, 0x001f, 0xffff, 0, 0xf81f};
  auto c = request(pixels);
  mipi_test_sdk::reset_calls();
  ASSERT_TRUE(actual.driver->supports_rgb565_transform());
  ASSERT_TRUE(actual.driver->draw_rgb565_transformed(c, sizeof(pixels), 1, 1, 3, 2));
  ASSERT_EQ(mipi_test_sdk::ppa_calls, 1u);
  ASSERT_EQ(mipi_test_sdk::draw_calls, 1u);
  EXPECT_EQ(mipi_test_sdk::no_copy_calls, 1u);
  EXPECT_TRUE(mipi_test_sdk::observed.owner_output);
  EXPECT_EQ(mipi_test_sdk::observed.mode, PPA_TRANS_MODE_BLOCKING);
  EXPECT_EQ(c.rotation_angle, PPA_SRM_ROTATION_ANGLE_0);
  EXPECT_EQ(c.out.buffer, reinterpret_cast<void *>(1));
  const Color colors[]{Color(255, 0, 0),     Color(0, 255, 0), Color(0, 0, 255),
                       Color(255, 255, 255), Color(0, 0, 0),   Color(255, 0, 255)};
  for (int y = 0; y < 2; y++)
    for (int x = 0; x < 3; x++)
      expected.driver->draw_pixel_at(1 + x, 1 + y, colors[y * 3 + x]);
  expected.driver->update();
  EXPECT_EQ(actual.panel->memory, expected.panel->memory);
}
INSTANTIATE_TEST_SUITE_P(OwnerRotation, MipiOwnedRotation,
                         ::testing::Values(display::DISPLAY_ROTATION_0_DEGREES, display::DISPLAY_ROTATION_90_DEGREES,
                                           display::DISPLAY_ROTATION_180_DEGREES,
                                           display::DISPLAY_ROTATION_270_DEGREES));
TEST_F(MipiOwned, GenericYuvInputStaysOnePassAndUsesItsActualByteDepth) {
  auto rig = new_owned();
  std::array<uint8_t, 24> pixels{};
  auto c = request(nullptr, 4, 4);
  c.in.buffer = pixels.data();
  c.in.srm_cm = PPA_SRM_COLOR_MODE_YUV420;
  mipi_test_sdk::reset_calls();
  EXPECT_FALSE(rig.driver->draw_rgb565_transformed(c, 23, 0, 0, 4, 4));
  EXPECT_EQ(mipi_test_sdk::ppa_calls, 0u);
  ASSERT_TRUE(rig.driver->draw_rgb565_transformed(c, pixels.size(), 0, 0, 4, 4));
  EXPECT_EQ(mipi_test_sdk::ppa_calls, 1u);
  EXPECT_EQ(mipi_test_sdk::draw_calls, 1u);
  EXPECT_EQ(mipi_test_sdk::observed.input_cm, PPA_SRM_COLOR_MODE_YUV420);
  EXPECT_EQ(mipi_test_sdk::observed.output_cm, PPA_SRM_COLOR_MODE_RGB565);
}
TEST_F(MipiOwned, InvalidRequestsCannotMutateOrRotateTwice) {
  auto rig = new_owned();
  uint16_t pixels[6]{};
  auto c = request(pixels);
  mipi_test_sdk::reset_calls();
  const auto before = rig.panel->memory;
  EXPECT_FALSE(rig.driver->draw_rgb565_transformed(c, sizeof(pixels) - 1, 0, 0, 3, 2));
  EXPECT_FALSE(rig.driver->draw_rgb565_transformed(c, sizeof(pixels), -1, 0, 3, 2));
  EXPECT_FALSE(rig.driver->draw_rgb565_transformed(c, sizeof(pixels), 7, 0, 3, 2));
  EXPECT_FALSE(rig.driver->draw_rgb565_transformed(c, sizeof(pixels), 0, 0, 4, 2));
  c.rotation_angle = PPA_SRM_ROTATION_ANGLE_90;
  EXPECT_FALSE(rig.driver->draw_rgb565_transformed(c, sizeof(pixels), 0, 0, 3, 2));
  c.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  c.scale_x = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(rig.driver->draw_rgb565_transformed(c, sizeof(pixels), 0, 0, 3, 2));
  c.scale_x = 1;
  c.in.buffer = rig.panel->cache.data();
  EXPECT_FALSE(rig.driver->draw_rgb565_transformed(c, rig.panel->size, 0, 0, 3, 2));
  EXPECT_EQ(mipi_test_sdk::ppa_calls, 0u);
  EXPECT_EQ(rig.panel->memory, before);
}
TEST_F(MipiOwned, DriverRechecksReadOnlyAdmissionAndReadbackIsBusyDuringTheTransaction) {
  auto rig = new_owned();
  uint16_t pixels[6]{};
  auto c = request(pixels);
  std::array<uint8_t, 64> target;
  target.fill(0xa5);
  bool allow = false;
  auto admitted = [](void *p) { return *static_cast<bool *>(p); };
  mipi_test_sdk::reset_calls();
  EXPECT_FALSE(rig.driver->draw_rgb565_transformed(c, sizeof(pixels), 0, 0, 3, 2, admitted, &allow));
  EXPECT_EQ(mipi_test_sdk::ppa_calls, 0u);
  allow = true;
  bool copied = true;
  mipi_test_sdk::on_ppa = [&] { copied = rig.driver->copy_frame_buffer_rgb565(target.data(), target.size()); };
  ASSERT_TRUE(rig.driver->draw_rgb565_transformed(c, sizeof(pixels), 0, 0, 3, 2, admitted, &allow));
  EXPECT_FALSE(copied);
  EXPECT_TRUE(std::all_of(target.begin(), target.end(), [](uint8_t b) { return b == 0xa5; }));
  mipi_test_sdk::on_ppa = {};
  ASSERT_TRUE(rig.driver->copy_frame_buffer_rgb565(target.data(), target.size()));
}
TEST_F(MipiOwned, CompletionWorksFromTaskAndIsrAndErrorsNeverWaitForMissingCallbacks) {
  for (bool isr : {false, true}) {
    auto rig = new_owned();
    uint16_t pixels[6]{};
    auto c = request(pixels);
    mipi_test_sdk::reset_calls();
    mipi_test_sdk::callback_isr = isr;
    ASSERT_TRUE(rig.driver->draw_rgb565_transformed(c, sizeof(pixels), 0, 0, 3, 2));
    EXPECT_EQ(mipi_test_sdk::from_isr_gives, isr ? 1u : 0u);
    EXPECT_EQ(mipi_test_sdk::ppa_calls, 1u);
  }
  auto rig = new_owned();
  uint16_t pixels[6]{};
  auto c = request(pixels);
  mipi_test_sdk::reset_calls();
  mipi_test_sdk::ppa_error = ESP_FAIL;
  EXPECT_FALSE(rig.driver->draw_rgb565_transformed(c, sizeof(pixels), 0, 0, 3, 2));
  EXPECT_EQ(mipi_test_sdk::draw_calls, 0u);
  mipi_test_sdk::ppa_error = ESP_OK;
  ASSERT_TRUE(rig.driver->draw_rgb565_transformed(c, sizeof(pixels), 0, 0, 3, 2));
}
TEST_F(MipiOwned, NormalBulkRotationKeepsTheExistingCanvasCoherent) {
  auto actual = new_owned(), expected = new_owned();
  actual.driver->set_rotation(display::DISPLAY_ROTATION_90_DEGREES);
  expected.driver->set_rotation(display::DISPLAY_ROTATION_90_DEGREES);
  actual.driver->draw_pixel_at(0, 0, Color(255, 255, 255));
  actual.driver->update();
  expected.driver->draw_pixel_at(0, 0, Color(255, 255, 255));
  expected.driver->update();
  uint16_t pixels[]{0xf800, 0x07e0};
  mipi_test_sdk::reset_calls();
  actual.driver->draw_pixels_at(1, 2, 2, 1, reinterpret_cast<uint8_t *>(pixels), display::COLOR_BITNESS_565, false);
  EXPECT_EQ(mipi_test_sdk::ppa_calls, 1u);
  expected.driver->draw_pixel_at(1, 2, Color(255, 0, 0));
  expected.driver->draw_pixel_at(2, 2, Color(0, 255, 0));
  for (auto *driver : {actual.driver, expected.driver}) {
    driver->draw_pixel_at(0, 0, Color(0, 0, 255));
    driver->draw_pixel_at(driver->get_width() - 1, driver->get_height() - 1, Color(0, 0, 255));
    driver->update();
  }
  EXPECT_EQ(actual.panel->memory, expected.panel->memory);
}
TEST_F(MipiOwned, StaleCompletionCannotReleaseTheNextBorrowEarly) {
  auto rig = new_owned();
  uint16_t pixels[32]{};
  for (unsigned i = 0; i < 32; i++)
    pixels[i] = 0x1357 + i;
  mipi_test_sdk::callback(rig.panel);  // A prior SDK edge that was not consumed.
  mipi_test_sdk::reset_calls();
  rig.driver->draw_pixels_at(0, 0, 8, 4, reinterpret_cast<uint8_t *>(pixels), display::COLOR_BITNESS_565, false);
  ASSERT_FALSE(mipi_test_sdk::pending.active);
  EXPECT_EQ(mipi_test_sdk::io_waits, 1u);
  EXPECT_EQ(std::memcmp(rig.panel->memory.data(), pixels, sizeof(pixels)), 0);
}
TEST_F(MipiOwned, RejectedPanelSubmitNeverWaitsForAnAbsentCallback) {
  auto rig = new_owned();
  uint16_t pixels[6]{};
  auto c = request(pixels);
  mipi_test_sdk::reset_calls();
  mipi_test_sdk::draw_error = ESP_FAIL;
  EXPECT_FALSE(rig.driver->draw_rgb565_transformed(c, sizeof(pixels), 0, 0, 3, 2));
  EXPECT_EQ(mipi_test_sdk::io_waits, 0u);
  EXPECT_FALSE(mipi_test_sdk::pending.active);
}
class MipiOutputRotation : public MipiOwned, public ::testing::WithParamInterface<display::DisplayRotation> {};
TEST_P(MipiOutputRotation, ReadbackAddsOnlyTheRequestedClockwiseRotation) {
  auto actual = new_owned();
  actual.driver->set_rotation(display::DISPLAY_ROTATION_270_DEGREES);
  const bool quarter =
      GetParam() == display::DISPLAY_ROTATION_90_DEGREES || GetParam() == display::DISPLAY_ROTATION_270_DEGREES;
  auto *reference = new MipiDsi(quarter ? 8 : 4, quarter ? 4 : 8, display::COLOR_BITNESS_565, 16);
  reference->set_auto_clear(false);
  reference->set_writer([](display::Display &) {});
  reference->setup();
  auto *expected = mipi_test_sdk::last_panel;
  reference->set_rotation(GetParam());
  const Color colors[]{Color(255, 0, 0),     Color(0, 255, 0), Color(0, 0, 255),
                       Color(255, 255, 255), Color(0, 0, 0),   Color(255, 0, 255)};
  for (unsigned y = 0; y < 8; y++)
    for (unsigned x = 0; x < 4; x++) {
      const auto color = colors[(x + 3 * y + 2 * x * y) % 6];
      actual.driver->draw_pixel_at(x, y, color);
      reference->draw_pixel_at(x, y, color);
    }
  actual.driver->update();
  reference->update();
  std::array<uint8_t, 64> target{};
  ASSERT_TRUE(actual.driver->copy_frame_buffer_rgb565(target.data(), target.size(), GetParam()));
  EXPECT_TRUE(std::equal(target.begin(), target.end(), expected->memory.begin()));
}
INSTANTIATE_TEST_SUITE_P(RequestedRotation, MipiOutputRotation,
                         ::testing::Values(display::DISPLAY_ROTATION_0_DEGREES, display::DISPLAY_ROTATION_90_DEGREES,
                                           display::DISPLAY_ROTATION_180_DEGREES,
                                           display::DISPLAY_ROTATION_270_DEGREES));
}  // namespace esphome::mipi_dsi::testing
#endif
