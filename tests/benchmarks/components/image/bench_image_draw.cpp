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
  BenchDisplay() { this->init_internal_(kWidth * kHeight); }
  void update() override {}
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return kWidth; }
  int get_height_internal() override { return kHeight; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override {
    this->buffer_[y * kWidth + x] = color.r ^ color.g ^ color.b;
  }
};

// Deterministic pseudo random pixel bytes; the values only need to vary so
// the chroma key and alpha branches see a mix of pixels.
static std::unique_ptr<uint8_t[]> make_pixels(size_t bytes) {
  auto data = std::make_unique<uint8_t[]>(bytes);
  uint32_t seed = 11897;
  for (size_t i = 0; i < bytes; i++) {
    seed = seed * 1103515245u + 12345u;
    data[i] = static_cast<uint8_t>(seed >> 16);
  }
  return data;
}

static void draw_image(benchmark::State &state, ImageType type, Transparency transparency, size_t bytes) {
  auto data = make_pixels(bytes);
  Image image(data.get(), kWidth, kHeight, type, transparency);
  BenchDisplay display;

  for (auto _ : state) {
    image.draw(0, 0, &display, display::COLOR_ON, display::COLOR_OFF);
    benchmark::DoNotOptimize(&display);
  }
  state.SetItemsProcessed(state.iterations() * kWidth * kHeight);
}

static void ImageDraw_Binary(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_BINARY, TRANSPARENCY_OPAQUE, (kWidth + 7) / 8 * kHeight);
}
BENCHMARK(ImageDraw_Binary);

static void ImageDraw_Grayscale(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_GRAYSCALE, TRANSPARENCY_OPAQUE, kWidth * kHeight);
}
BENCHMARK(ImageDraw_Grayscale);

static void ImageDraw_RGB565(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE, kWidth * kHeight * 2);
}
BENCHMARK(ImageDraw_RGB565);

static void ImageDraw_RGB(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_RGB, TRANSPARENCY_OPAQUE, kWidth * kHeight * 3);
}
BENCHMARK(ImageDraw_RGB);

static void ImageDraw_RGB_Alpha(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_RGB, TRANSPARENCY_ALPHA_CHANNEL, kWidth * kHeight * 4);
}
BENCHMARK(ImageDraw_RGB_Alpha);

}  // namespace esphome::image::benchmarks
