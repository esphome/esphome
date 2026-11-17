#include <benchmark/benchmark.h>

#include <cstdint>
#include <memory>

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/runtime_image/runtime_image.h"

namespace esphome::runtime_image::benchmarks {

static constexpr int kWidth = 128;
static constexpr int kHeight = 96;

// One byte per pixel frame buffer with no hardware behind it, so the
// measurement is the image draw plus the DisplayBuffer pixel path every
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

// A runtime image filled the way a decoder fills it, one draw_pixel() per
// pixel, then published as decoded so draw() uses the buffer.
class BenchImage : public RuntimeImage {
 public:
  BenchImage(image::ImageType type, image::Transparency transparency, bool big_endian)
      : RuntimeImage(JPEG, type, transparency, nullptr, big_endian, kWidth, kHeight) {}

  bool fill() {
    if (this->resize(kWidth, kHeight) == 0)
      return false;
    uint32_t seed = 11897;
    for (int y = 0; y < kHeight; y++) {
      for (int x = 0; x < kWidth; x++) {
        seed = seed * 1103515245u + 12345u;
        // Every fourth pixel is transparent so the chroma key cases hit both branches
        const uint8_t alpha = (x & 3) == 0 ? 0x00 : 0xFF;
        this->draw_pixel(x, y, Color(seed >> 24, seed >> 16, seed >> 8, alpha));
      }
    }
    this->width_ = this->buffer_width_;
    this->height_ = this->buffer_height_;
    this->data_start_ = this->buffer_;
    return true;
  }
};

// The decoder side: writing every pixel of the image into its buffer.
static void fill_image(benchmark::State &state, image::ImageType type, image::Transparency transparency) {
  BenchImage image(type, transparency, false);
  for (auto _ : state) {
    if (!image.fill()) {
      state.SkipWithError("buffer allocation failed");
      return;
    }
    benchmark::DoNotOptimize(image.get_data_start());
  }
  state.SetItemsProcessed(state.iterations() * kWidth * kHeight);
}

// The display side: drawing the decoded image onto a buffered display.
static void draw_image(benchmark::State &state, image::ImageType type, image::Transparency transparency,
                       bool big_endian = false) {
  BenchImage image(type, transparency, big_endian);
  if (!image.fill()) {
    state.SkipWithError("buffer allocation failed");
    return;
  }
  BenchDisplay display;
  for (auto _ : state) {
    image.draw(0, 0, &display, display::COLOR_ON, display::COLOR_OFF);
  }
  state.SetItemsProcessed(state.iterations() * kWidth * kHeight);
}

static void RuntimeImageFill_RGB565(benchmark::State &state) {
  fill_image(state, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE);
}
BENCHMARK(RuntimeImageFill_RGB565);

static void RuntimeImageFill_RGB565_ChromaKey(benchmark::State &state) {
  fill_image(state, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_CHROMA_KEY);
}
BENCHMARK(RuntimeImageFill_RGB565_ChromaKey);

static void RuntimeImageFill_RGB(benchmark::State &state) {
  fill_image(state, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
}
BENCHMARK(RuntimeImageFill_RGB);

static void RuntimeImageDraw_RGB565(benchmark::State &state) {
  draw_image(state, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE);
}
BENCHMARK(RuntimeImageDraw_RGB565);

static void RuntimeImageDraw_RGB565_BigEndian(benchmark::State &state) {
  draw_image(state, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE, true);
}
BENCHMARK(RuntimeImageDraw_RGB565_BigEndian);

static void RuntimeImageDraw_RGB565_ChromaKey(benchmark::State &state) {
  draw_image(state, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_CHROMA_KEY);
}
BENCHMARK(RuntimeImageDraw_RGB565_ChromaKey);

static void RuntimeImageDraw_RGB(benchmark::State &state) {
  draw_image(state, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
}
BENCHMARK(RuntimeImageDraw_RGB);

static void RuntimeImageDraw_Grayscale(benchmark::State &state) {
  draw_image(state, image::IMAGE_TYPE_GRAYSCALE, image::TRANSPARENCY_OPAQUE);
}
BENCHMARK(RuntimeImageDraw_Grayscale);

}  // namespace esphome::runtime_image::benchmarks
