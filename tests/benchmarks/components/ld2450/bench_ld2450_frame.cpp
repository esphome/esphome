#include <benchmark/benchmark.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/ld2450/ld2450.h"
#include "esphome/components/sensor/sensor.h"
#include "fake_uart.h"

namespace esphome::ld2450::benchmarks {

using esphome::benchmarks::FakeUART;

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;
static constexpr size_t FRAME_SIZE = 30;

// Periodic frames with one moving target at two positions
static constexpr uint8_t FRAME_A[FRAME_SIZE] = {0xAA, 0xFF, 0x03, 0x00, 0x0E, 0x03, 0xB1, 0x86, 0x10, 0x00,
                                                0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xCC};
static constexpr uint8_t FRAME_B[FRAME_SIZE] = {0xAA, 0xFF, 0x03, 0x00, 0x2A, 0x03, 0xC8, 0x86, 0x10, 0x00,
                                                0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xCC};

// --- loop() parsing one periodic frame, no entities attached ---

static void LD2450_PeriodicFrame(benchmark::State &state) {
  FakeUART uart;
  LD2450Component radar;
  radar.set_uart_parent(&uart);
  radar.set_link_timeout(60000);
  // Bring the link up outside the timed loop
  uart.load(FRAME_B, FRAME_SIZE);
  radar.loop();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uart.load(FRAME_A, FRAME_SIZE);
      radar.loop();
    }
    benchmark::DoNotOptimize(uart.available());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(LD2450_PeriodicFrame);

// --- loop() parsing a moving target and publishing its sensors ---

static void LD2450_PeriodicFrameSensors(benchmark::State &state) {
  FakeUART uart;
  LD2450Component radar;
  radar.set_uart_parent(&uart);
  radar.set_link_timeout(60000);
  sensor::Sensor x, y, speed, distance, angle, count;
  binary_sensor::BinarySensor presence;
  radar.set_move_x_sensor(0, &x);
  radar.set_move_y_sensor(0, &y);
  radar.set_move_speed_sensor(0, &speed);
  radar.set_move_distance_sensor(0, &distance);
  radar.set_move_angle_sensor(0, &angle);
  radar.set_target_count_sensor(&count);
  radar.set_target_binary_sensor(&presence);
  uart.load(FRAME_B, FRAME_SIZE);
  radar.loop();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uart.load((i & 1) ? FRAME_B : FRAME_A, FRAME_SIZE);
      radar.loop();
    }
    benchmark::DoNotOptimize(x.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(LD2450_PeriodicFrameSensors);

}  // namespace esphome::ld2450::benchmarks
