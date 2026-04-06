#include <benchmark/benchmark.h>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_buffer.h"

namespace esphome::api::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 2000;

// Helper: encode a message into an APIBuffer for reuse in decode benchmarks.
// Optimization barriers are applied to the decode target objects via
// DoNotOptimize/ClobberMemory, not to this buffer.
template<typename T> static APIBuffer encode_message(const T &msg) {
  APIBuffer buffer;
  uint32_t size = msg.calculate_size();
  buffer.resize(size);
  ProtoWriteBuffer writer(&buffer, 0);
  msg.encode(writer);
  return buffer;
}

/// Force a pointer through an asm barrier so the compiler cannot
/// prove its contents are unchanged across iterations.
/// benchmark::DoNotOptimize/ClobberMemory are insufficient under
/// CodSpeed's valgrind-based instrumentation.
static void escape(void *p) { asm volatile("" : : "g"(p) : "memory"); }

// --- HelloRequest decode (string + varint fields) ---

static void Decode_HelloRequest(benchmark::State &state) {
  HelloRequest source;
  source.client_info = StringRef::from_lit("aioesphomeapi");
  source.api_version_major = 1;
  source.api_version_minor = 10;
  auto encoded = encode_message(source);
  auto *data = encoded.data();
  auto size = encoded.size();
  benchmark::DoNotOptimize(data);
  benchmark::DoNotOptimize(size);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      HelloRequest msg;
      escape(&msg);
      msg.decode(data, size);
      escape(&msg);
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_HelloRequest);

// --- SwitchCommandRequest decode (simple command) ---

static void Decode_SwitchCommandRequest(benchmark::State &state) {
  SwitchCommandRequest source;
  source.key = 0x12345678;
  source.state = true;
  auto encoded = encode_message(source);
  auto *data = encoded.data();
  auto size = encoded.size();
  benchmark::DoNotOptimize(data);
  benchmark::DoNotOptimize(size);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      SwitchCommandRequest msg;
      escape(&msg);
      msg.decode(data, size);
      escape(&msg);
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
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
  auto encoded = encode_message(source);
  auto *data = encoded.data();
  auto size = encoded.size();
  benchmark::DoNotOptimize(data);
  benchmark::DoNotOptimize(size);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      LightCommandRequest msg;
      escape(&msg);
      msg.decode(data, size);
      escape(&msg);
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_LightCommandRequest);

}  // namespace esphome::api::benchmarks
