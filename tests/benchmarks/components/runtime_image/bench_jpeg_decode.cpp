#include <benchmark/benchmark.h>

#include <vector>

#include "esphome/components/runtime_image/runtime_image.h"
#include "jpeg_96x96.h"

namespace esphome::benchmarks {

using runtime_image::RuntimeImage;

// A fixed-size image so every decode scales the same JPEG into the same buffer
class BenchImage : public RuntimeImage {
 public:
  BenchImage(image::ImageType type, int width, int height)
      : RuntimeImage(runtime_image::JPEG, type, image::TRANSPARENCY_OPAQUE, nullptr, false, width, height) {}
};

// One full decode session, fed the way online_image feeds a download
static bool decode(BenchImage &img, std::vector<uint8_t> &buffer) {
  if (!img.begin_decode(buffer.size(), runtime_image::JPEG)) {
    return false;
  }
  size_t offset = 0;
  while (offset < buffer.size()) {
    const int consumed = img.feed_data(buffer.data() + offset, buffer.size() - offset);
    if (consumed <= 0) {
      return false;
    }
    offset += consumed;
  }
  return img.end_decode();
}

static void run(benchmark::State &state, image::ImageType type, int width, int height, const uint8_t *jpeg = JPEG_96X96,
                size_t size = JPEG_96X96_SIZE) {
  BenchImage img(type, width, height);
  std::vector<uint8_t> buffer(jpeg, jpeg + size);
  for (auto _ : state) {
    if (!decode(img, buffer)) {
      state.SkipWithError("decode failed");
      return;
    }
    benchmark::DoNotOptimize(img.get_data_start());
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(width) * height);
}

// --- JPEG into an RGB565 image: same size, half size and double size ---

static void JpegDecode_Rgb565_Native(benchmark::State &state) { run(state, image::IMAGE_TYPE_RGB565, 96, 96); }
BENCHMARK(JpegDecode_Rgb565_Native);

static void JpegDecode_Rgb565_Half(benchmark::State &state) { run(state, image::IMAGE_TYPE_RGB565, 48, 48); }
BENCHMARK(JpegDecode_Rgb565_Half);

static void JpegDecode_Rgb565_Double(benchmark::State &state) { run(state, image::IMAGE_TYPE_RGB565, 192, 192); }
BENCHMARK(JpegDecode_Rgb565_Double);

// --- Grayscale JPEG into a grayscale image: same size, half size and double size ---

static void gray(benchmark::State &state, int width, int height) {
  run(state, image::IMAGE_TYPE_GRAYSCALE, width, height, JPEG_96X96_GRAY, JPEG_96X96_GRAY_SIZE);
}

static void JpegDecode_Grayscale_Native(benchmark::State &state) { gray(state, 96, 96); }
BENCHMARK(JpegDecode_Grayscale_Native);

static void JpegDecode_Grayscale_Half(benchmark::State &state) { gray(state, 48, 48); }
BENCHMARK(JpegDecode_Grayscale_Half);

static void JpegDecode_Grayscale_Double(benchmark::State &state) { gray(state, 192, 192); }
BENCHMARK(JpegDecode_Grayscale_Double);

// --- Colour JPEG into a grayscale image, which goes through JPEGDEC's RGB8888 output ---

static void JpegDecode_Grayscale_FromColour(benchmark::State &state) {
  run(state, image::IMAGE_TYPE_GRAYSCALE, 96, 96);
}
BENCHMARK(JpegDecode_Grayscale_FromColour);

// --- JPEG into an RGB image at the same size, the generic per-pixel path for reference ---

static void JpegDecode_Rgb_Native(benchmark::State &state) { run(state, image::IMAGE_TYPE_RGB, 96, 96); }
BENCHMARK(JpegDecode_Rgb_Native);

}  // namespace esphome::benchmarks
