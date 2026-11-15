#include <benchmark/benchmark.h>

#include "esphome/core/frame_reader.h"
#include "esphome/core/line_reader.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

static constexpr char NMEA_GGA[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

// A 30 byte frame with a 4 byte header and a 2 byte tail
static constexpr uint8_t FRAME_30[30] = {0xAA, 0xFF, 0x03, 0x00, 0x0E, 0x03, 0xB1, 0x86, 0x10, 0x00,
                                         0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xCC};

// Four A02YYUW distance frames among noise bytes; the doubled 0xFF fails the checksum and resyncs onto the
// frame behind it. Ends on a whole frame, so every pass starts clean
static constexpr uint8_t ULTRASONIC_NOISY[] = {0x12, 0xFF, 0x07, 0xA1, 0xA7, 0x34, 0xFF, 0xFF, 0x07, 0xA1, 0xA7,
                                               0x00, 0xFF, 0x07, 0xA1, 0xA7, 0x56, 0xFF, 0x07, 0xA1, 0xA7};

// --- LineReader: one GGA sentence fed byte by byte ---

static void LineReader_NmeaLine(benchmark::State &state) {
  LineReader<96> reader;
  const auto *bytes = reinterpret_cast<const uint8_t *>(NMEA_GGA);
  size_t total = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(bytes);
    for (int i = 0; i < kInnerIterations; i++) {
      for (size_t j = 0; j < sizeof(NMEA_GGA) - 1; j++) {
        size_t len;
        if (reader.feed(bytes[j], len) != nullptr)
          total += len;
      }
    }
    benchmark::DoNotOptimize(total);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(LineReader_NmeaLine);

// --- FrameReader: a 30 byte frame behind a 4 byte header ---

static void FrameReader_30ByteFrame(benchmark::State &state) {
  FrameReader<sizeof(FRAME_30), 0xAA, 0xFF, 0x03, 0x00> reader;
  const uint8_t *bytes = FRAME_30;
  uint32_t frames = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(bytes);
    for (int i = 0; i < kInnerIterations; i++) {
      for (size_t j = 0; j < sizeof(FRAME_30); j++) {
        if (reader.feed(bytes[j]) != nullptr)
          frames++;
      }
    }
    benchmark::DoNotOptimize(frames);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(FrameReader_30ByteFrame);

// --- FrameReader: 4 byte checksummed frames, skipping noise and resyncing on a bad checksum ---

static void FrameReader_UltrasonicNoisy(benchmark::State &state) {
  FrameReader<4, 0xFF> reader;
  const uint8_t *bytes = ULTRASONIC_NOISY;
  uint32_t frames = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(bytes);
    for (int i = 0; i < kInnerIterations; i++) {
      for (size_t j = 0; j < sizeof(ULTRASONIC_NOISY); j++) {
        const uint8_t *frame = reader.feed(bytes[j]);
        if (frame == nullptr)
          continue;
        if (frame[3] == static_cast<uint8_t>(frame[0] + frame[1] + frame[2])) {
          frames++;
        } else {
          reader.resync();
        }
      }
    }
    benchmark::DoNotOptimize(frames);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(FrameReader_UltrasonicNoisy);

}  // namespace esphome::benchmarks
