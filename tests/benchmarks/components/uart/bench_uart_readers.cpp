#include <benchmark/benchmark.h>

#include "esphome/components/uart/uart.h"
#include "fake_uart.h"

namespace esphome::uart::benchmarks {

using esphome::benchmarks::FakeUART;

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

// One second of a GPS module's NMEA output
static constexpr char NMEA_BURST[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n"
                                     "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n"
                                     "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39\r\n"
                                     "$GPGSV,2,1,08,01,40,083,46,02,17,308,41,12,07,344,39,14,22,228,45*75\r\n"
                                     "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48\r\n";

// --- read_available(): drain a whole NMEA burst through a byte-sum callback ---

static void UART_ReadAvailable_NmeaBurst(benchmark::State &state) {
  FakeUART uart;
  UARTDevice dev(&uart);
  uint32_t sum = 0;

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uart.load(reinterpret_cast<const uint8_t *>(NMEA_BURST), sizeof(NMEA_BURST) - 1);
      dev.read_available([&](uint8_t b) { sum += b; });
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(UART_ReadAvailable_NmeaBurst);

// --- discard_available(): drop a 256 byte backlog ---

// Two virtual calls per byte, available() and read_byte(): this measures that per-byte call overhead
static void UART_DiscardAvailable(benchmark::State &state) {
  uint8_t backlog[256]{};
  FakeUART uart;
  UARTDevice dev(&uart);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uart.load(backlog, sizeof(backlog));
      dev.discard_available();
    }
    benchmark::DoNotOptimize(uart.available());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(UART_DiscardAvailable);

}  // namespace esphome::uart::benchmarks
