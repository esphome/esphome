#include <benchmark/benchmark.h>

#include <cstdint>
#include <memory>

#include "esphome/components/display/display_buffer.h"

namespace esphome::display::benchmarks {

static constexpr int kWidth = 128;
static constexpr int kHeight = 96;
// Square frame so a rotated block stays inside it.
static constexpr int kStride = kWidth > kHeight ? kWidth : kHeight;

// A buffered display without its own draw_pixels_at(), so every block goes
// through the base class fallback and the DisplayBuffer pixel path.
class BenchDisplay : public DisplayBuffer {
 public:
  BenchDisplay() : frame_(std::make_unique<uint8_t[]>(kStride * kStride)) {}
  void update() override {}
  DisplayType get_display_type() override { return DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return kStride; }
  int get_height_internal() override { return kStride; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override { this->frame_[y * kStride + x] = color.r; }

  std::unique_ptr<uint8_t[]> frame_;
};

// Deterministic pseudo random pixel bytes, sized for the widest format.
static std::unique_ptr<uint8_t[]> make_pixels() {
  constexpr size_t bytes = kWidth * kHeight * 3;
  auto data = std::make_unique<uint8_t[]>(bytes);
  uint32_t seed = 11897;
  for (size_t i = 0; i < bytes; i++) {
    seed = seed * 1103515245u + 12345u;
    data[i] = static_cast<uint8_t>(seed >> 16);
  }
  return data;
}

// One full block the way LVGL hands a display its rendering.
static void draw_block(benchmark::State &state, ColorBitness bitness, bool big_endian,
                       DisplayRotation rotation = DISPLAY_ROTATION_0_DEGREES) {
  auto data = make_pixels();
  BenchDisplay display;
  display.set_rotation(rotation);
  for (auto _ : state) {
    display.draw_pixels_at(0, 0, kWidth, kHeight, data.get(), bitness, big_endian, 0, 0, 0);
  }
  state.SetItemsProcessed(state.iterations() * kWidth * kHeight);
}

static void DrawPixelsAt_RGB565_BigEndian(benchmark::State &state) { draw_block(state, COLOR_BITNESS_565, true); }
BENCHMARK(DrawPixelsAt_RGB565_BigEndian);

static void DrawPixelsAt_RGB565_LittleEndian(benchmark::State &state) { draw_block(state, COLOR_BITNESS_565, false); }
BENCHMARK(DrawPixelsAt_RGB565_LittleEndian);

static void DrawPixelsAt_RGB888(benchmark::State &state) { draw_block(state, COLOR_BITNESS_888, true); }
BENCHMARK(DrawPixelsAt_RGB888);

static void DrawPixelsAt_RGB332(benchmark::State &state) { draw_block(state, COLOR_BITNESS_332, true); }
BENCHMARK(DrawPixelsAt_RGB332);

static void DrawPixelsAt_RGB565_Rotated(benchmark::State &state) {
  draw_block(state, COLOR_BITNESS_565, true, DISPLAY_ROTATION_90_DEGREES);
}
BENCHMARK(DrawPixelsAt_RGB565_Rotated);

static void DrawPixelsAt_RGB888_Rotated(benchmark::State &state) {
  draw_block(state, COLOR_BITNESS_888, true, DISPLAY_ROTATION_90_DEGREES);
}
BENCHMARK(DrawPixelsAt_RGB888_Rotated);

static void DrawPixelsAt_RGB332_Rotated(benchmark::State &state) {
  draw_block(state, COLOR_BITNESS_332, true, DISPLAY_ROTATION_90_DEGREES);
}
BENCHMARK(DrawPixelsAt_RGB332_Rotated);

}  // namespace esphome::display::benchmarks
