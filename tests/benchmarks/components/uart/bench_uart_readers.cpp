#include <benchmark/benchmark.h>

#include <cstring>

#include "esphome/components/uart/uart.h"
#include "esphome/core/frame_reader.h"
#include "esphome/core/line_reader.h"
#include "fake_uart.h"

namespace esphome::uart::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

// One second of a GPS module's NMEA output
static constexpr char NMEA_BURST[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n"
                                     "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n"
                                     "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39\r\n"
                                     "$GPGSV,2,1,08,01,40,083,46,02,17,308,41,12,07,344,39,14,22,228,45*75\r\n"
                                     "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48\r\n";
static constexpr size_t NMEA_BURST_LEN = sizeof(NMEA_BURST) - 1;

// LD2450 periodic frame with one tracked target
static constexpr uint8_t LD2450_FRAME[30] = {0xAA, 0xFF, 0x03, 0x00, 0x0E, 0x03, 0xB1, 0x86, 0x10, 0x00,
                                             0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xCC};

// A02YYUW distance frames with line noise; the doubled 0xFF fails the checksum and resyncs
static constexpr uint8_t ULTRASONIC_NOISY[] = {0x12, 0xFF, 0x07, 0xA1, 0xA7, 0x34, 0xFF, 0xFF, 0x07, 0xA1,
                                               0xA7, 0x00, 0xFF, 0x07, 0xA1, 0xA7, 0x56, 0xFF, 0x07, 0xA1};

// --- LineReader: one GGA sentence fed byte by byte ---

static void LineReader_NmeaLine(benchmark::State &state) {
  LineReader<96> reader;
  const auto *bytes = reinterpret_cast<const uint8_t *>(NMEA_BURST);
  const size_t gga_len = std::strchr(NMEA_BURST, '\n') - NMEA_BURST + 1;
  size_t total = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(bytes);  // keep the input opaque to the optimizer
    for (int i = 0; i < kInnerIterations; i++) {
      for (size_t j = 0; j < gga_len; j++) {
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

static void FrameReader_LD2450Frame(benchmark::State &state) {
  FrameReader<sizeof(LD2450_FRAME), 0xAA, 0xFF, 0x03, 0x00> reader;
  const uint8_t *bytes = LD2450_FRAME;
  uint32_t frames = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(bytes);
    for (int i = 0; i < kInnerIterations; i++) {
      for (size_t j = 0; j < sizeof(LD2450_FRAME); j++) {
        if (reader.feed(bytes[j]) != nullptr)
          frames++;
      }
    }
    benchmark::DoNotOptimize(frames);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(FrameReader_LD2450Frame);

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

// --- read_available(): drain a whole NMEA burst into a LineReader ---

static void UART_ReadAvailable_NmeaBurst(benchmark::State &state) {
  FakeUART uart(reinterpret_cast<const uint8_t *>(NMEA_BURST), NMEA_BURST_LEN);
  UARTDevice dev(&uart);
  LineReader<96> reader;
  uint32_t lines = 0;

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uart.rewind();
      dev.read_available([&](uint8_t b) {
        if (reader.feed(b) != nullptr)
          lines++;
      });
    }
    benchmark::DoNotOptimize(lines);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(UART_ReadAvailable_NmeaBurst);

// --- discard_available(): drop a 256 byte backlog ---

static void UART_DiscardAvailable(benchmark::State &state) {
  static constexpr size_t backlog_len = 256;
  uint8_t backlog[backlog_len];
  std::memset(backlog, 0x5A, sizeof(backlog));
  FakeUART uart(backlog, backlog_len);
  UARTDevice dev(&uart);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uart.rewind();
      dev.discard_available();
    }
    benchmark::DoNotOptimize(uart.available());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(UART_DiscardAvailable);

}  // namespace esphome::uart::benchmarks
