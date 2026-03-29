#include <benchmark/benchmark.h>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_buffer.h"

namespace esphome::api::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 2000;

// Helper: encode a message into a buffer and return the raw data/size.
// The returned pointer/size are forced through an asm barrier so the
// compiler cannot prove their values are compile-time constants.
template<typename T> static APIBuffer encode_message(const T &msg) {
  APIBuffer buffer;
  uint32_t size = msg.calculate_size();
  buffer.resize(size);
  ProtoWriteBuffer writer(&buffer, 0);
  msg.encode(writer);
  return buffer;
}

/// Force a pointer and size through an asm barrier so the compiler
/// treats them as opaque runtime values.
static void opaquify(const uint8_t *&data, size_t &size) { asm volatile("" : "+r"(data), "+r"(size) : : "memory"); }

// --- HelloRequest decode (string + varint fields) ---

static void Decode_HelloRequest(benchmark::State &state) {
  HelloRequest source;
  source.client_info = StringRef::from_lit("aioesphomeapi");
  source.api_version_major = 1;
  source.api_version_minor = 10;
  auto encoded = encode_message(source);
  const uint8_t *data = encoded.data();
  size_t size = encoded.size();
  opaquify(data, size);

  for (auto _ : state) {
    HelloRequest msg;
    for (int i = 0; i < kInnerIterations; i++) {
      msg.decode(data, size);
    }
    benchmark::DoNotOptimize(msg);
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
  const uint8_t *data = encoded.data();
  size_t size = encoded.size();
  opaquify(data, size);

  for (auto _ : state) {
    SwitchCommandRequest msg;
    for (int i = 0; i < kInnerIterations; i++) {
      msg.decode(data, size);
    }
    benchmark::DoNotOptimize(msg);
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
  const uint8_t *data = encoded.data();
  size_t size = encoded.size();
  opaquify(data, size);

  for (auto _ : state) {
    LightCommandRequest msg;
    for (int i = 0; i < kInnerIterations; i++) {
      msg.decode(data, size);
    }
    benchmark::DoNotOptimize(msg);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_LightCommandRequest);

}  // namespace esphome::api::benchmarks
