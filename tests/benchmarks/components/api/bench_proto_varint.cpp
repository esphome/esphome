#include <benchmark/benchmark.h>

#include "esphome/components/api/proto.h"
#include "esphome/components/api/api_buffer.h"

namespace esphome::api::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 2000;

// --- ProtoVarInt::parse() benchmarks ---

static void ProtoVarInt_Parse_SingleByte(benchmark::State &state) {
  uint8_t buf[] = {0x42};  // value = 66

  for (auto _ : state) {
    ProtoVarIntResult result{};
    for (int i = 0; i < kInnerIterations; i++) {
      result = ProtoVarInt::parse(buf, sizeof(buf));
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ProtoVarInt_Parse_SingleByte);

static void ProtoVarInt_Parse_TwoByte(benchmark::State &state) {
  uint8_t buf[] = {0x80, 0x01};  // value = 128

  for (auto _ : state) {
    ProtoVarIntResult result{};
    for (int i = 0; i < kInnerIterations; i++) {
      result = ProtoVarInt::parse(buf, sizeof(buf));
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ProtoVarInt_Parse_TwoByte);

static void ProtoVarInt_Parse_FiveByte(benchmark::State &state) {
  uint8_t buf[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F};

  for (auto _ : state) {
    ProtoVarIntResult result{};
    for (int i = 0; i < kInnerIterations; i++) {
      result = ProtoVarInt::parse(buf, sizeof(buf));
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ProtoVarInt_Parse_FiveByte);

// --- Varint encoding benchmarks ---

static void Encode_Varint_Small(benchmark::State &state) {
  APIBuffer buffer;
  buffer.resize(16);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ProtoWriteBuffer writer(&buffer, 0);
      writer.encode_varint_raw(42);
    }
    benchmark::DoNotOptimize(buffer.data());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Encode_Varint_Small);

static void Encode_Varint_Large(benchmark::State &state) {
  APIBuffer buffer;
  buffer.resize(16);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ProtoWriteBuffer writer(&buffer, 0);
      writer.encode_varint_raw(300);
    }
    benchmark::DoNotOptimize(buffer.data());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Encode_Varint_Large);

static void Encode_Varint_MaxUint32(benchmark::State &state) {
  APIBuffer buffer;
  buffer.resize(16);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ProtoWriteBuffer writer(&buffer, 0);
      writer.encode_varint_raw(0xFFFFFFFF);
    }
    benchmark::DoNotOptimize(buffer.data());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Encode_Varint_MaxUint32);

// --- ProtoSize::varint() benchmarks ---

static void ProtoSize_Varint_Small(benchmark::State &state) {
  // Use varying input to prevent constant folding.
  // Values 0-127 all take 1 byte but the compiler can't prove that.
  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += ProtoSize::varint(static_cast<uint32_t>(i) & 0x7F);
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ProtoSize_Varint_Small);

static void ProtoSize_Varint_Large(benchmark::State &state) {
  // Use varying input to prevent constant folding.
  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += ProtoSize::varint(0xFFFF0000 | static_cast<uint32_t>(i));
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ProtoSize_Varint_Large);

}  // namespace esphome::api::benchmarks
