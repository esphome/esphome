#include <benchmark/benchmark.h>

#include <cstdint>
#include <memory>

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/image/image.h"

namespace esphome::image::benchmarks {

static constexpr int kWidth = 128;
static constexpr int kHeight = 96;

// One byte per pixel frame buffer with no hardware behind it, so the
// measurement is Image::draw() plus the DisplayBuffer pixel path every
// buffered display shares.
class BenchDisplay : public display::DisplayBuffer {
 public:
  BenchDisplay() : frame_(std::make_unique<uint8_t[]>(kWidth * kHeight)) {}
  void update() override {}
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return kWidth; }
  int get_height_internal() override { return kHeight; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override { this->frame_[y * kWidth + x] = color.r; }

  std::unique_ptr<uint8_t[]> frame_;
};

// Deterministic pseudo random pixel bytes so the alpha branch sees a mix of
// pixels. Sized for the widest format, the image reads only what it needs.
static std::unique_ptr<uint8_t[]> make_pixels() {
  constexpr size_t bytes = kWidth * kHeight * 4;
  auto data = std::make_unique<uint8_t[]>(bytes);
  uint32_t seed = 11897;
  for (size_t i = 0; i < bytes; i++) {
    seed = seed * 1103515245u + 12345u;
    data[i] = static_cast<uint8_t>(seed >> 16);
  }
  return data;
}

static void draw_image(benchmark::State &state, ImageType type, Transparency transparency,
                       display::DisplayRotation rotation = display::DISPLAY_ROTATION_0_DEGREES) {
  auto data = make_pixels();
  Image image(data.get(), kWidth, kHeight, type, transparency);
  BenchDisplay display;
  display.set_rotation(rotation);

  for (auto _ : state) {
    image.draw(0, 0, &display, display::COLOR_ON, display::COLOR_OFF);
  }
  state.SetItemsProcessed(state.iterations() * kWidth * kHeight);
}

static void ImageDraw_Binary(benchmark::State &state) { draw_image(state, IMAGE_TYPE_BINARY, TRANSPARENCY_OPAQUE); }
BENCHMARK(ImageDraw_Binary);

static void ImageDraw_Grayscale(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_GRAYSCALE, TRANSPARENCY_OPAQUE);
}
BENCHMARK(ImageDraw_Grayscale);

static void ImageDraw_RGB565(benchmark::State &state) { draw_image(state, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE); }
BENCHMARK(ImageDraw_RGB565);

static void ImageDraw_RGB(benchmark::State &state) { draw_image(state, IMAGE_TYPE_RGB, TRANSPARENCY_OPAQUE); }
BENCHMARK(ImageDraw_RGB);

static void ImageDraw_RGB_Rotated(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_RGB, TRANSPARENCY_OPAQUE, display::DISPLAY_ROTATION_90_DEGREES);
}
BENCHMARK(ImageDraw_RGB_Rotated);

static void ImageDraw_RGB_Alpha(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_RGB, TRANSPARENCY_ALPHA_CHANNEL);
}
BENCHMARK(ImageDraw_RGB_Alpha);

}  // namespace esphome::image::benchmarks
