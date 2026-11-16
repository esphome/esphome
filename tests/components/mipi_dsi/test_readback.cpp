#if defined(USE_HOST) && defined(MIPI_TEST_IDF_MAJOR)
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <thread>
#include <tuple>

#include "esphome/components/mipi_dsi/mipi_dsi.h"
#include "esphome/core/hal.h"
#include "mipi_test_sdk.h"

namespace esphome::mipi_dsi::testing {
struct DisplayRig {
  MipiDsi *display;
  TestDpiPanel *panel;
};
DisplayRig make_rig(bool big_endian, display::ColorBitness depth = display::COLOR_BITNESS_565) {
  // Components and SDK handles have MCU lifetime. Keep these fixtures reachable
  // for the test process lifetime.
  auto *driver = new MipiDsi(8, 4, depth, depth == display::COLOR_BITNESS_565 ? 16 : 24);
  driver->set_big_endian(big_endian);
  driver->set_auto_clear(false);
  driver->set_writer([](display::Display &) {});
  driver->setup();
  return {driver, mipi_test_sdk::last_panel};
}
DisplayRig &rgb_rig(bool big_endian) {
  static DisplayRig little = make_rig(false);
  static DisplayRig big = make_rig(true);
  return big_endian ? big : little;
}
std::array<uint8_t, 64> pattern(int width, bool big_endian = false) {
  static constexpr uint16_t COLORS[]{0xf800, 0x07e0, 0x001f, 0xffff, 0, 0xf81f};
  std::array<uint8_t, 64> bytes{};
  for (size_t index = 0; index < 32; ++index) {
    const size_t x = index % width, y = index / width;
    const uint16_t value = COLORS[(x + 3 * y + 2 * x * y) % 6];
    bytes[index * 2 + (big_endian ? 1 : 0)] = value & 0xff;
    bytes[index * 2 + (big_endian ? 0 : 1)] = value >> 8;
  }
  return bytes;
}
void expect_unchanged(const std::array<uint8_t, 80> &target) {
  EXPECT_TRUE(std::all_of(target.begin(), target.end(), [](uint8_t value) { return value == 0xa5; }));
}

class MipiReadback : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    // The same public platform initialization production performs before
    // components run.
    arch_init();
  }

 protected:
  void SetUp() override { mipi_test_sdk::reset_calls(); }
};
class MipiReadbackPixels : public MipiReadback,
                           public ::testing::WithParamInterface<std::tuple<display::DisplayRotation, bool>> {};

TEST_P(MipiReadbackPixels, PublicDrawCopiesCompletedDpiPixelsInLogicalLittleEndianOrder) {
  const auto [rotation, big_endian] = MipiReadbackPixels::GetParam();
  auto &rig = rgb_rig(big_endian);
  rig.display->set_rotation(rotation);
  mipi_test_sdk::reset_calls();
  const auto source = pattern(rig.display->get_width(), big_endian);
  const auto expected = pattern(rig.display->get_width());
  rig.display->draw_pixels_at(0, 0, rig.display->get_width(), rig.display->get_height(), source.data(),
                              display::COLOR_BITNESS_565, big_endian);
  ASSERT_FALSE(mipi_test_sdk::pending.active);
  ASSERT_GT(mipi_test_sdk::draw_calls, 0u);
  const auto memory_before = rig.panel->memory;
  const auto waits_before = mipi_test_sdk::blocking_takes;
  const auto tries_before = mipi_test_sdk::try_takes;
  const auto cache_before = mipi_test_sdk::cache_calls;
  std::array<uint8_t, 80> target;
  target.fill(0xa5);
  ASSERT_TRUE(rig.display->copy_frame_buffer_rgb565(target.data() + 8, target.size() - 8));
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), target.begin() + 8));
  EXPECT_TRUE(std::all_of(target.begin(), target.begin() + 8, [](uint8_t value) { return value == 0xa5; }));
  EXPECT_TRUE(std::all_of(target.begin() + 72, target.end(), [](uint8_t value) { return value == 0xa5; }));
  EXPECT_EQ(rig.panel->memory, memory_before);
  EXPECT_EQ(mipi_test_sdk::get_calls, 0u);
  EXPECT_EQ(mipi_test_sdk::cache_calls, cache_before + 1u);
  EXPECT_EQ(mipi_test_sdk::blocking_takes, waits_before);
  EXPECT_EQ(mipi_test_sdk::try_takes, tries_before + 1u);
}

// GoogleTest registration creates a generated non-const global.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
INSTANTIATE_TEST_SUITE_P(
    RotationAndInputEndian, MipiReadbackPixels,
    ::testing::Combine(::testing::Values(display::DISPLAY_ROTATION_0_DEGREES, display::DISPLAY_ROTATION_90_DEGREES,
                                         display::DISPLAY_ROTATION_180_DEGREES, display::DISPLAY_ROTATION_270_DEGREES),
                       ::testing::Bool()));

TEST_F(MipiReadback, DirectBlockDrawingCanBeReadWithoutCreatingABufferedWriter) {
  static DisplayRig direct = make_rig(false);
  const auto image = pattern(direct.display->get_width());
  direct.display->draw_pixels_at(0, 0, 8, 4, image.data(), display::COLOR_BITNESS_565, false);
  std::array<uint8_t, 80> target;
  target.fill(0xa5);
  ASSERT_TRUE(direct.display->copy_frame_buffer_rgb565(target.data(), target.size()));
  EXPECT_TRUE(std::equal(image.begin(), image.end(), target.begin()));
}

TEST_F(MipiReadback, InvalidOutputAndMissingSetupDoNotTouchStorage) {
  alignas(uint16_t) std::array<uint8_t, 80> target;
  target.fill(0xa5);
  auto &rig = rgb_rig(false);
  EXPECT_FALSE(rig.display->copy_frame_buffer_rgb565(nullptr, target.size()));
  EXPECT_FALSE(rig.display->copy_frame_buffer_rgb565(target.data(), 63));
  EXPECT_FALSE(rig.display->copy_frame_buffer_rgb565(target.data() + 1, target.size() - 1,
                                                     display::DISPLAY_ROTATION_90_DEGREES));
  MipiDsi unconfigured(8, 4, display::COLOR_BITNESS_565, 16);
  EXPECT_FALSE(unconfigured.copy_frame_buffer_rgb565(target.data(), target.size()));
  expect_unchanged(target);
}

TEST_F(MipiReadback, SdkAndCacheErrorsLeaveCallerStorageUntouched) {
  auto &rig = rgb_rig(false);
  rig.display->set_rotation(display::DISPLAY_ROTATION_0_DEGREES);
  std::array<uint8_t, 80> target;
  target.fill(0xa5);
  mipi_test_sdk::get_error = ESP_FAIL;
  const auto missing = make_rig(false);
  EXPECT_FALSE(missing.display->copy_frame_buffer_rgb565(target.data(), target.size()));
  expect_unchanged(target);
  mipi_test_sdk::get_error = ESP_OK;
  mipi_test_sdk::null_frame = true;
  const auto empty = make_rig(false);
  EXPECT_FALSE(empty.display->copy_frame_buffer_rgb565(target.data(), target.size()));
  expect_unchanged(target);
  mipi_test_sdk::null_frame = false;
  mipi_test_sdk::cache_error = ESP_FAIL;
  EXPECT_FALSE(rig.display->copy_frame_buffer_rgb565(target.data(), target.size()));
  expect_unchanged(target);
}

TEST_F(MipiReadback, UnsupportedFormatAndAliasingAreRejectedBeforeCacheSync) {
  static DisplayRig rgb888 = make_rig(false, display::COLOR_BITNESS_888);
  std::array<uint8_t, 80> target;
  target.fill(0xa5);
  mipi_test_sdk::reset_calls();
  EXPECT_FALSE(rgb888.display->copy_frame_buffer_rgb565(target.data(), target.size()));
  auto &rig = rgb_rig(false);
  const auto cache_before = rig.panel->cache;
  EXPECT_FALSE(rig.display->copy_frame_buffer_rgb565(rig.panel->cache.data(), rig.panel->size));
  EXPECT_EQ(rig.panel->cache, cache_before);
  EXPECT_EQ(mipi_test_sdk::cache_calls, 0u);
  expect_unchanged(target);
}

TEST_F(MipiReadback, PendingBufferedDrawingMustCompleteThroughThePublicUpdate) {
  auto &rig = rgb_rig(false);
  rig.display->set_rotation(display::DISPLAY_ROTATION_0_DEGREES);
  std::array<uint8_t, 80> target;
  target.fill(0xa5);
  rig.display->fill(Color(255, 0, 0));
  EXPECT_FALSE(rig.display->copy_frame_buffer_rgb565(target.data(), target.size()));
  expect_unchanged(target);
  rig.display->update();
  ASSERT_TRUE(rig.display->copy_frame_buffer_rgb565(target.data(), target.size()));
  for (size_t index = 0; index < 32; ++index) {
    EXPECT_EQ(target[index * 2], 0u);
    EXPECT_EQ(target[index * 2 + 1], 0xf8u);
  }
}

TEST_F(MipiReadback, WriterReentryAndBackgroundCallAreRejected) {
  auto &rig = rgb_rig(false);
  std::array<uint8_t, 80> target;
  target.fill(0xa5);
  bool writer_result = true;
  rig.display->set_writer(
      [&](display::Display &) { writer_result = rig.display->copy_frame_buffer_rgb565(target.data(), target.size()); });
  rig.display->update();
  rig.display->set_writer([](display::Display &) {});
  EXPECT_FALSE(writer_result);
  bool background_result = true;
  std::thread background(
      [&]() { background_result = rig.display->copy_frame_buffer_rgb565(target.data(), target.size()); });
  background.join();
  EXPECT_FALSE(background_result);
  expect_unchanged(target);
}
}  // namespace esphome::mipi_dsi::testing
#endif
