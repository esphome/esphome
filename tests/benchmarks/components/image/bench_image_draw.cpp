#include <benchmark/benchmark.h>

#include <cstdint>
#include <cstring>
#include <memory>

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/image/image.h"

namespace esphome::image::benchmarks {

static constexpr int kWidth = 128;
static constexpr int kHeight = 96;
// Square frame so a rotated draw of the image stays inside it.
static constexpr int kStride = kWidth > kHeight ? kWidth : kHeight;

// One byte per pixel frame buffer with no hardware behind it, so the
// measurement is Image::draw() plus the DisplayBuffer pixel path every
// buffered display shares. Everything fits the host cache, so this counts
// the per pixel work; the memory locality win only shows on hardware.
class BenchDisplay : public display::DisplayBuffer {
 public:
  BenchDisplay() : frame_(std::make_unique<uint8_t[]>(kStride * kStride)) {}
  void update() override {}
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return kStride; }
  int get_height_internal() override { return kStride; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override { this->frame_[y * kStride + x] = color.r; }

  std::unique_ptr<uint8_t[]> frame_;

 public:
  uint8_t pixel(int x, int y) const { return this->frame_[y * kStride + x]; }
};

// Models a driver with a bulk path: rows of little endian RGB565 are copied
// straight into a 16 bit frame, as the TFT drivers do for their own format.
class BulkBenchDisplay : public BenchDisplay {
 public:
  BulkBenchDisplay() : frame565_(std::make_unique<uint16_t[]>(kStride * kStride)) {}
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override {
    if (bitness != display::COLOR_BITNESS_565 || big_endian) {
      BenchDisplay::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset, x_pad);
      return;
    }
    const size_t stride = x_offset + w + x_pad;
    for (int y = 0; y < h; y++) {
      memcpy(this->frame565_.get() + (y_start + y) * kStride + x_start, ptr + ((y_offset + y) * stride + x_offset) * 2,
             w * 2);
    }
  }

 protected:
  std::unique_ptr<uint16_t[]> frame565_;
};

// Draws the image at rotation 0 and 90 and checks the second frame is the
// transpose of the first, which is what a wrong traversal order would break.
static bool frames_match(const Image &image) {
  BenchDisplay plain;
  BenchDisplay rotated;
  rotated.set_rotation(display::DISPLAY_ROTATION_90_DEGREES);
  const_cast<Image &>(image).draw(0, 0, &plain, display::COLOR_ON, display::COLOR_OFF);
  const_cast<Image &>(image).draw(0, 0, &rotated, display::COLOR_ON, display::COLOR_OFF);
  for (int y = 0; y < kHeight; y++) {
    for (int x = 0; x < kWidth; x++) {
      // DisplayBuffer maps (x, y) to (width - y - 1, x) at 90 degrees
      if (plain.pixel(x, y) != rotated.pixel(kStride - y - 1, x))
        return false;
    }
  }
  return true;
}

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
  if (!frames_match(image)) {
    state.SkipWithError("rotated draw is not the transpose of the plain draw");
    return;
  }

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

// The same opaque RGB565 image on a display with a bulk path.
static void ImageDraw_RGB565_Bulk(benchmark::State &state) {
  auto data = make_pixels();
  Image image(data.get(), kWidth, kHeight, IMAGE_TYPE_RGB565, TRANSPARENCY_OPAQUE);
  BulkBenchDisplay display;
  for (auto _ : state) {
    image.draw(0, 0, &display, display::COLOR_ON, display::COLOR_OFF);
  }
  state.SetItemsProcessed(state.iterations() * kWidth * kHeight);
}
BENCHMARK(ImageDraw_RGB565_Bulk);

static void ImageDraw_RGB_Rotated(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_RGB, TRANSPARENCY_OPAQUE, display::DISPLAY_ROTATION_90_DEGREES);
}
BENCHMARK(ImageDraw_RGB_Rotated);

static void ImageDraw_Binary_Transparent(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_BINARY, TRANSPARENCY_CHROMA_KEY);
}
BENCHMARK(ImageDraw_Binary_Transparent);

static void ImageDraw_Grayscale_ChromaKey(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_GRAYSCALE, TRANSPARENCY_CHROMA_KEY);
}
BENCHMARK(ImageDraw_Grayscale_ChromaKey);

static void ImageDraw_Grayscale_Alpha(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_GRAYSCALE, TRANSPARENCY_ALPHA_CHANNEL);
}
BENCHMARK(ImageDraw_Grayscale_Alpha);

static void ImageDraw_RGB_Alpha(benchmark::State &state) {
  draw_image(state, IMAGE_TYPE_RGB, TRANSPARENCY_ALPHA_CHANNEL);
}
BENCHMARK(ImageDraw_RGB_Alpha);

}  // namespace esphome::image::benchmarks
