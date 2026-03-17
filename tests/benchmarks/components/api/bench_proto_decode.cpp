#include <benchmark/benchmark.h>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_buffer.h"

namespace esphome::api::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 1000;

// --- HelloRequest decode (string + varint fields) ---

static void Decode_HelloRequest(benchmark::State &state) {
  // Manually encoded HelloRequest:
  // field 1 (string): "aioesphomeapi"
  // field 2 (varint): 1  (api_version_major)
  // field 3 (varint): 10 (api_version_minor)
  uint8_t encoded[] = {
      0x0A, 0x0D,                                                         // field 1, length 13
      'a',  'i',  'o', 'e', 's', 'p', 'h', 'o', 'm', 'e', 'a', 'p', 'i',  // "aioesphomeapi"
      0x10, 0x01,                                                         // field 2, value 1
      0x18, 0x0A,                                                         // field 3, value 10
  };

  for (auto _ : state) {
    HelloRequest msg;
    for (int i = 0; i < kInnerIterations; i++) {
      msg.decode(encoded, sizeof(encoded));
    }
    benchmark::DoNotOptimize(msg.api_version_major);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_HelloRequest);

// --- SwitchCommandRequest decode (simple command) ---

static void Decode_SwitchCommandRequest(benchmark::State &state) {
  // field 1 (fixed32): key = 0x12345678
  // field 2 (varint): state = true
  uint8_t encoded[] = {
      0x0D, 0x78, 0x56, 0x34, 0x12,  // field 1, fixed32
      0x10, 0x01,                    // field 2, varint true
  };

  for (auto _ : state) {
    SwitchCommandRequest msg;
    for (int i = 0; i < kInnerIterations; i++) {
      msg.decode(encoded, sizeof(encoded));
    }
    benchmark::DoNotOptimize(msg.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_SwitchCommandRequest);

// --- LightCommandRequest decode (complex command with many fields) ---

static void Decode_LightCommandRequest(benchmark::State &state) {
  uint8_t encoded[] = {
      // field 1: key (fixed32) = 0x11223344
      0x0D,
      0x44,
      0x33,
      0x22,
      0x11,
      // field 2: has_state (varint) = true
      0x10,
      0x01,
      // field 3: state (varint) = true
      0x18,
      0x01,
      // field 4: has_brightness (varint) = true
      0x20,
      0x01,
      // field 5: brightness (fixed32/float) = 0.8
      0x2D,
      0xCD,
      0xCC,
      0x4C,
      0x3F,
      // field 9: has_rgb (varint) = true
      0x48,
      0x01,
      // field 10: red (fixed32/float) = 1.0
      0x55,
      0x00,
      0x00,
      0x80,
      0x3F,
      // field 11: green (fixed32/float) = 0.5
      0x5D,
      0x00,
      0x00,
      0x00,
      0x3F,
      // field 12: blue (fixed32/float) = 0.2
      0x65,
      0xCD,
      0xCC,
      0x4C,
      0x3E,
      // field 20: has_effect (varint) = true
      0xA0,
      0x01,
      0x01,
      // field 21: effect (string) = "rainbow"
      0xAA,
      0x01,
      0x07,
      'r',
      'a',
      'i',
      'n',
      'b',
      'o',
      'w',
  };

  for (auto _ : state) {
    LightCommandRequest msg;
    for (int i = 0; i < kInnerIterations; i++) {
      msg.decode(encoded, sizeof(encoded));
    }
    benchmark::DoNotOptimize(msg.brightness);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_LightCommandRequest);

}  // namespace esphome::api::benchmarks
