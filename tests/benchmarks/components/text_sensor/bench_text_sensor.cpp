#include <benchmark/benchmark.h>

#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::text_sensor::benchmarks {

static constexpr int kInnerIterations = 2000;

// --- publish_state(const char *) with short string, value changes each time ---
// Exercises: memcmp check (mismatch), string assign, callback dispatch.

static void TextSensorPublish_Short_Changing(benchmark::State &state) {
  TextSensor sensor;

  // Pre-populate with different short strings
  const char *values[] = {"192.168.1.1", "192.168.1.2", "192.168.1.3", "192.168.1.4"};

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(values[i % 4]);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(TextSensorPublish_Short_Changing);

// --- publish_state(const char *) with short string, same value (dedup path) ---
// Exercises: memcmp check (match), skips string assign.

static void TextSensorPublish_Short_NoChange(benchmark::State &state) {
  TextSensor sensor;
  sensor.publish_state("192.168.1.100");

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state("192.168.1.100");
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(TextSensorPublish_Short_NoChange);

// --- publish_state with longer string (firmware version, MAC address) ---
// Exercises: memcmp on longer strings, string assign with potential realloc.

static void TextSensorPublish_Long_Changing(benchmark::State &state) {
  TextSensor sensor;

  const char *values[] = {
      "2025.12.0-dev (Jan 15 2025, 10:30:00)",
      "2025.12.1-dev (Feb 20 2025, 14:45:00)",
      "2025.12.2-dev (Mar 10 2025, 08:15:00)",
      "2025.12.3-dev (Apr  5 2025, 16:00:00)",
  };

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(values[i % 4]);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(TextSensorPublish_Long_Changing);

// --- publish_state with callback ---
// Measures callback dispatch overhead for text sensors.

static void TextSensorPublish_WithCallback(benchmark::State &state) {
  TextSensor sensor;

  uint64_t callback_count = 0;
  sensor.add_on_state_callback([&callback_count](const std::string &) { callback_count++; });

  const char *values[] = {"192.168.1.1", "192.168.1.2", "192.168.1.3", "192.168.1.4"};

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(values[i % 4]);
    }
    benchmark::DoNotOptimize(callback_count);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(TextSensorPublish_WithCallback);

// --- publish_state(const char *, size_t) direct ---
// The lowest-level overload, avoids strlen.

static void TextSensorPublish_WithLen(benchmark::State &state) {
  TextSensor sensor;

  static constexpr const char *values[] = {"192.168.1.1", "192.168.1.2", "192.168.1.3", "192.168.1.4"};
  static constexpr size_t lens[] = {11, 11, 11, 11};

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      sensor.publish_state(values[i % 4], lens[i % 4]);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(TextSensorPublish_WithLen);

}  // namespace esphome::text_sensor::benchmarks
