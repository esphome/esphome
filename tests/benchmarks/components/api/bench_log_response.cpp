#include <benchmark/benchmark.h>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_buffer.h"

namespace esphome::api::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

// Typical log line: "[12:34:56][D][sensor:094]: 'Temperature': Sending state 23.50000 with 1 decimals of accuracy"
static constexpr const char *kTypicalLogLine =
    "[12:34:56][D][sensor:094]: 'Temperature': Sending state 23.50000 with 1 decimals of accuracy";

// Short log line: "[12:34:56][I][app:029]: Running..."
static constexpr const char *kShortLogLine = "[12:34:56][I][app:029]: Running...";

// --- Encode ---

static void Encode_LogResponse_Typical(benchmark::State &state) {
  APIBuffer buffer;
  SubscribeLogsResponse msg;
  msg.level = enums::LOG_LEVEL_DEBUG;
  msg.set_message(reinterpret_cast<const uint8_t *>(kTypicalLogLine), strlen(kTypicalLogLine));
  uint32_t size = msg.calculate_size();
  buffer.resize(size);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ProtoWriteBuffer writer(&buffer, 0);
      msg.encode(writer);
    }
    benchmark::DoNotOptimize(buffer.data());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Encode_LogResponse_Typical);

static void Encode_LogResponse_Short(benchmark::State &state) {
  APIBuffer buffer;
  SubscribeLogsResponse msg;
  msg.level = enums::LOG_LEVEL_INFO;
  msg.set_message(reinterpret_cast<const uint8_t *>(kShortLogLine), strlen(kShortLogLine));
  uint32_t size = msg.calculate_size();
  buffer.resize(size);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ProtoWriteBuffer writer(&buffer, 0);
      msg.encode(writer);
    }
    benchmark::DoNotOptimize(buffer.data());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Encode_LogResponse_Short);

// --- Calculate Size ---

static void CalculateSize_LogResponse_Typical(benchmark::State &state) {
  SubscribeLogsResponse msg;
  msg.level = enums::LOG_LEVEL_DEBUG;
  msg.set_message(reinterpret_cast<const uint8_t *>(kTypicalLogLine), strlen(kTypicalLogLine));

  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += msg.calculate_size();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CalculateSize_LogResponse_Typical);

// --- Calc + Encode (steady state) ---

static void CalcAndEncode_LogResponse_Typical(benchmark::State &state) {
  APIBuffer buffer;
  SubscribeLogsResponse msg;
  msg.level = enums::LOG_LEVEL_DEBUG;
  msg.set_message(reinterpret_cast<const uint8_t *>(kTypicalLogLine), strlen(kTypicalLogLine));

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uint32_t size = msg.calculate_size();
      buffer.resize(size);
      ProtoWriteBuffer writer(&buffer, 0);
      msg.encode(writer);
    }
    benchmark::DoNotOptimize(buffer.data());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CalcAndEncode_LogResponse_Typical);

// --- Calc + Encode (fresh allocation each time) ---

static void CalcAndEncode_LogResponse_Typical_Fresh(benchmark::State &state) {
  SubscribeLogsResponse msg;
  msg.level = enums::LOG_LEVEL_DEBUG;
  msg.set_message(reinterpret_cast<const uint8_t *>(kTypicalLogLine), strlen(kTypicalLogLine));

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      APIBuffer buffer;
      uint32_t size = msg.calculate_size();
      buffer.resize(size);
      ProtoWriteBuffer writer(&buffer, 0);
      msg.encode(writer);
      benchmark::DoNotOptimize(buffer.data());
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CalcAndEncode_LogResponse_Typical_Fresh);

}  // namespace esphome::api::benchmarks
