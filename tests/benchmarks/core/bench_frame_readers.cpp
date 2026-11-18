#include <benchmark/benchmark.h>

#include "esphome/core/frame_reader.h"
#include "esphome/core/helpers.h"
#include "esphome/core/line_reader.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

static constexpr char NMEA_GGA[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

// A 30 byte frame with a 4 byte header and a 2 byte tail
static constexpr uint8_t FRAME_30[30] = {0xAA, 0xFF, 0x03, 0x00, 0x0E, 0x03, 0xB1, 0x86, 0x10, 0x00,
                                         0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xCC};

// Four A02YYUW frames among noise; the doubled 0xFF fails the checksum and resyncs onto the next frame
static constexpr uint8_t ULTRASONIC_NOISY[] = {0x12, 0xFF, 0x07, 0xA1, 0xA7, 0x34, 0xFF, 0xFF, 0x07, 0xA1, 0xA7,
                                               0x00, 0xFF, 0x07, 0xA1, 0xA7, 0x56, 0xFF, 0x07, 0xA1, 0xA7};

// Pioneer WT shaped frames (A5 01 header, whole-frame length at byte 7) of 12, 20 and 12 bytes among noise
static constexpr uint8_t LENGTH_FRAMES[] = {0x00, 0xA5, 0x01, 0x01, 0x23, 0x00, 0xA5, 0x00, 0x0C, 0x11, 0x22, 0x33,
                                            0x44, 0xA5, 0x01, 0x01, 0x23, 0x00, 0xA5, 0x00, 0x14, 0x11, 0x22, 0x33,
                                            0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0x7E, 0xA5, 0x01,
                                            0x01, 0x23, 0x00, 0xA5, 0x00, 0x0C, 0x11, 0x22, 0x33, 0x44};

// --- LineReader: one GGA sentence fed byte by byte ---

static void LineReader_NmeaLine(benchmark::State &state) {
  LineReader<96> reader;
  const auto *bytes = reinterpret_cast<const uint8_t *>(NMEA_GGA);
  uint32_t lines = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(bytes);
    for (int i = 0; i < kInnerIterations; i++) {
      for (size_t j = 0; j < sizeof(NMEA_GGA) - 1; j++) {
        char *line = reader.feed(bytes[j]);
        if (line == nullptr)
          continue;
        // Keeps the line copy and its NUL write
        benchmark::DoNotOptimize(line);
        lines++;
      }
    }
    benchmark::DoNotOptimize(lines);
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
        if (frame[3] == sum_bytes(frame, 3)) {
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

// --- LengthFrameReader: frames of different lengths among noise ---

static void LengthFrameReader_MixedFrames(benchmark::State &state) {
  LengthFrameReader<255, FrameFormat<FrameLengthU8<7>, 0xA5, 0x01>> reader;
  const uint8_t *bytes = LENGTH_FRAMES;
  uint32_t frames = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(bytes);
    for (int i = 0; i < kInnerIterations; i++) {
      for (size_t j = 0; j < sizeof(LENGTH_FRAMES); j++) {
        const uint8_t *frame = reader.feed(bytes[j]);
        if (frame == nullptr)
          continue;
        benchmark::DoNotOptimize(frame);
        frames++;
      }
    }
    benchmark::DoNotOptimize(frames);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(LengthFrameReader_MixedFrames);

}  // namespace esphome::benchmarks
