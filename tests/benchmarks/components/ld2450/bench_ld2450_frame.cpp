#include <benchmark/benchmark.h>

#include <cstring>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/ld2450/ld2450.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::ld2450::benchmarks {

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

// Serves one frame per loop() pass, as the radar's 10 Hz stream does.
class FrameUART : public uart::UARTComponent {
 public:
  void load(const uint8_t *frame) {
    this->frame_ = frame;
    this->pos_ = 0;
  }
  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override { return false; }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > FRAME_SIZE - this->pos_)
      return false;
    std::memcpy(data, this->frame_ + this->pos_, len);
    this->pos_ += len;
    return true;
  }
  size_t available() override { return FRAME_SIZE - this->pos_; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}

 protected:
  const uint8_t *frame_{FRAME_A};
  size_t pos_{FRAME_SIZE};
};

class BenchSensor : public sensor::Sensor {
 public:
  explicit BenchSensor(const char *name) { this->configure_entity_(name, 0x12345678, 0); }
};

// --- loop() parsing one periodic frame, no entities attached ---

static void LD2450_PeriodicFrame(benchmark::State &state) {
  FrameUART uart;
  LD2450Component radar;
  radar.set_uart_parent(&uart);
  radar.set_link_timeout(60000);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uart.load(FRAME_A);
      radar.loop();
    }
    benchmark::DoNotOptimize(uart.available());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(LD2450_PeriodicFrame);

// --- loop() parsing a moving target and publishing its sensors ---

static void LD2450_PeriodicFrameSensors(benchmark::State &state) {
  FrameUART uart;
  LD2450Component radar;
  radar.set_uart_parent(&uart);
  radar.set_link_timeout(60000);
  BenchSensor x("x"), y("y"), speed("speed"), distance("distance"), angle("angle"), count("count");
  binary_sensor::BinarySensor presence;
  radar.set_move_x_sensor(0, &x);
  radar.set_move_y_sensor(0, &y);
  radar.set_move_speed_sensor(0, &speed);
  radar.set_move_distance_sensor(0, &distance);
  radar.set_move_angle_sensor(0, &angle);
  radar.set_target_count_sensor(&count);
  radar.set_target_binary_sensor(&presence);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uart.load((i & 1) ? FRAME_B : FRAME_A);
      radar.loop();
    }
    benchmark::DoNotOptimize(x.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(LD2450_PeriodicFrameSensors);

}  // namespace esphome::ld2450::benchmarks
