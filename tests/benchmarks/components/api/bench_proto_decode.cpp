#include <benchmark/benchmark.h>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_buffer.h"

namespace esphome::api::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 2000;

// Helper: encode a message into a buffer, then copy the bytes into a
// plain array so the compiler cannot trace them back to compile-time constants.
// Returns {heap-allocated copy, size}.  Caller owns the memory.
template<typename T> static std::pair<uint8_t *, size_t> encode_to_heap(const T &msg) {
  APIBuffer buffer;
  uint32_t size = msg.calculate_size();
  buffer.resize(size);
  ProtoWriteBuffer writer(&buffer, 0);
  msg.encode(writer);
  // Copy to a separate heap allocation so the compiler cannot prove
  // the contents are constant (the APIBuffer internals are visible).
  auto *copy = new uint8_t[size];
  std::memcpy(copy, buffer.data(), size);
  return {copy, size};
}

// --- HelloRequest decode (string + varint fields) ---

static void Decode_HelloRequest(benchmark::State &state) {
  HelloRequest source;
  source.client_info = StringRef::from_lit("aioesphomeapi");
  source.api_version_major = 1;
  source.api_version_minor = 10;
  auto [data, size] = encode_to_heap(source);
  benchmark::DoNotOptimize(size);

  for (auto _ : state) {
    HelloRequest msg;
    for (int i = 0; i < kInnerIterations; i++) {
      msg.decode(data, size);
      benchmark::ClobberMemory();
    }
    benchmark::DoNotOptimize(msg);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
  delete[] data;
}
BENCHMARK(Decode_HelloRequest);

// --- SwitchCommandRequest decode (simple command) ---

static void Decode_SwitchCommandRequest(benchmark::State &state) {
  SwitchCommandRequest source;
  source.key = 0x12345678;
  source.state = true;
  auto [data, size] = encode_to_heap(source);
  benchmark::DoNotOptimize(size);

  for (auto _ : state) {
    SwitchCommandRequest msg;
    for (int i = 0; i < kInnerIterations; i++) {
      msg.decode(data, size);
      benchmark::ClobberMemory();
    }
    benchmark::DoNotOptimize(msg);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
  delete[] data;
}
BENCHMARK(Decode_SwitchCommandRequest);

// --- LightCommandRequest decode (complex command with many fields) ---

static void Decode_LightCommandRequest(benchmark::State &state) {
  LightCommandRequest source;
  source.key = 0x11223344;
  source.has_state = true;
  source.state = true;
  source.has_brightness = true;
  source.brightness = 0.8f;
  source.has_rgb = true;
  source.red = 1.0f;
  source.green = 0.5f;
  source.blue = 0.2f;
  source.has_effect = true;
  source.effect = StringRef::from_lit("rainbow");
  auto [data, size] = encode_to_heap(source);
  benchmark::DoNotOptimize(size);

  for (auto _ : state) {
    LightCommandRequest msg;
    for (int i = 0; i < kInnerIterations; i++) {
      msg.decode(data, size);
      benchmark::ClobberMemory();
    }
    benchmark::DoNotOptimize(msg);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
  delete[] data;
}
BENCHMARK(Decode_LightCommandRequest);

}  // namespace esphome::api::benchmarks
