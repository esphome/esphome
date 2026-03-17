#include <benchmark/benchmark.h>

#include "esphome/components/api/proto.h"
#include "esphome/components/api/api_buffer.h"

namespace esphome::api::benchmarks {

// --- ProtoVarInt::parse() benchmarks ---

static void BM_ProtoVarInt_Parse_SingleByte(benchmark::State &state) {
  // Single-byte varint (0-127) — the most common case (fast path)
  uint8_t buf[] = {0x42};  // value = 66

  for (auto _ : state) {
    auto result = ProtoVarInt::parse(buf, sizeof(buf));
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_ProtoVarInt_Parse_SingleByte);

static void BM_ProtoVarInt_Parse_TwoByte(benchmark::State &state) {
  // Two-byte varint (128-16383)
  uint8_t buf[] = {0x80, 0x01};  // value = 128

  for (auto _ : state) {
    auto result = ProtoVarInt::parse(buf, sizeof(buf));
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_ProtoVarInt_Parse_TwoByte);

static void BM_ProtoVarInt_Parse_FiveByte(benchmark::State &state) {
  // Five-byte varint (max uint32 = 4294967295)
  uint8_t buf[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F};

  for (auto _ : state) {
    auto result = ProtoVarInt::parse(buf, sizeof(buf));
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_ProtoVarInt_Parse_FiveByte);

// --- Varint encoding benchmarks ---

static void BM_Encode_Varint_Small(benchmark::State &state) {
  // Value < 128 — single byte fast path
  APIBuffer buffer;
  buffer.resize(16);

  for (auto _ : state) {
    ProtoWriteBuffer writer(&buffer, 0);
    writer.encode_varint_raw(42);
    benchmark::DoNotOptimize(buffer.data());
  }
}
BENCHMARK(BM_Encode_Varint_Small);

static void BM_Encode_Varint_Large(benchmark::State &state) {
  // Value > 128 — multi-byte slow path
  APIBuffer buffer;
  buffer.resize(16);

  for (auto _ : state) {
    ProtoWriteBuffer writer(&buffer, 0);
    writer.encode_varint_raw(300);
    benchmark::DoNotOptimize(buffer.data());
  }
}
BENCHMARK(BM_Encode_Varint_Large);

static void BM_Encode_Varint_MaxUint32(benchmark::State &state) {
  APIBuffer buffer;
  buffer.resize(16);

  for (auto _ : state) {
    ProtoWriteBuffer writer(&buffer, 0);
    writer.encode_varint_raw(0xFFFFFFFF);
    benchmark::DoNotOptimize(buffer.data());
  }
}
BENCHMARK(BM_Encode_Varint_MaxUint32);

// --- ProtoSize::varint() benchmarks ---

static void BM_ProtoSize_Varint_Small(benchmark::State &state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(ProtoSize::varint(42));
  }
}
BENCHMARK(BM_ProtoSize_Varint_Small);

static void BM_ProtoSize_Varint_Large(benchmark::State &state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(ProtoSize::varint(0xFFFFFFFF));
  }
}
BENCHMARK(BM_ProtoSize_Varint_Large);

}  // namespace esphome::api::benchmarks
