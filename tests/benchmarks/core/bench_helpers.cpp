#include <benchmark/benchmark.h>
#include <cinttypes>
#include <cstdio>

#include "esphome/core/helpers.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 2000;

// --- random_float() ---

static void RandomFloat(benchmark::State &state) {
  for (auto _ : state) {
    float result = 0.0f;
    for (int i = 0; i < kInnerIterations; i++) {
      result += random_float();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(RandomFloat);

// --- random_uint32() ---

static void RandomUint32(benchmark::State &state) {
  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += random_uint32();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(RandomUint32);

// --- format_hex_to() - 6 bytes (MAC address sized) ---

static void FormatHexTo_6Bytes(benchmark::State &state) {
  const uint8_t data[] = {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45};
  char buffer[13];  // 6 * 2 + 1
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      format_hex_to(buffer, data, 6);
    }
    benchmark::DoNotOptimize(buffer);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(FormatHexTo_6Bytes);

// --- format_hex_to() - 16 bytes (UUID sized) ---

static void FormatHexTo_16Bytes(benchmark::State &state) {
  const uint8_t data[] = {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89,
                          0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
  char buffer[33];  // 16 * 2 + 1
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      format_hex_to(buffer, data, 16);
    }
    benchmark::DoNotOptimize(buffer);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(FormatHexTo_16Bytes);

// --- format_hex_to() - 100 bytes (large payload) ---

static void FormatHexTo_100Bytes(benchmark::State &state) {
  uint8_t data[100];
  for (int i = 0; i < 100; i++) {
    data[i] = static_cast<uint8_t>(i);
  }
  char buffer[201];  // 100 * 2 + 1
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      format_hex_to(buffer, data, 100);
    }
    benchmark::DoNotOptimize(buffer);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(FormatHexTo_100Bytes);

// --- format_hex_pretty_to() - 6 bytes with ':' separator ---

static void FormatHexPrettyTo_6Bytes(benchmark::State &state) {
  const uint8_t data[] = {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45};
  char buffer[18];  // 6 * 3
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      format_hex_pretty_to(buffer, data, 6);
    }
    benchmark::DoNotOptimize(buffer);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(FormatHexPrettyTo_6Bytes);

// --- format_mac_addr_upper() ---

static void FormatMacAddrUpper(benchmark::State &state) {
  const uint8_t mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  char buffer[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      format_mac_addr_upper(mac, buffer);
    }
    benchmark::DoNotOptimize(buffer);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(FormatMacAddrUpper);

// --- fnv1_hash() - short string ---

static void Fnv1Hash_Short(benchmark::State &state) {
  const char *str = "sensor.temperature";
  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result ^= fnv1_hash(str);
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Fnv1Hash_Short);

// --- fnv1_hash() - long string ---

static void Fnv1Hash_Long(benchmark::State &state) {
  const char *str = "binary_sensor.living_room_motion_sensor_occupancy_detected";
  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result ^= fnv1_hash(str);
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Fnv1Hash_Long);

// --- fnv1a_hash() - short string ---
// Use DoNotOptimize on the input pointer to prevent constexpr evaluation

static void Fnv1aHash_Short(benchmark::State &state) {
  const char *str = "sensor.temperature";
  benchmark::DoNotOptimize(str);
  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result ^= fnv1a_hash(str);
      benchmark::ClobberMemory();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Fnv1aHash_Short);

// --- fnv1a_hash() - long string ---

static void Fnv1aHash_Long(benchmark::State &state) {
  const char *str = "binary_sensor.living_room_motion_sensor_occupancy_detected";
  benchmark::DoNotOptimize(str);
  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result ^= fnv1a_hash(str);
      benchmark::ClobberMemory();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Fnv1aHash_Long);

// --- fnv1_hash_object_id() - typical entity name ---

static void Fnv1HashObjectId(benchmark::State &state) {
  char name[] = "Living Room Temperature Sensor";
  size_t len = sizeof(name) - 1;
  benchmark::DoNotOptimize(name);
  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result ^= fnv1_hash_object_id(name, len);
      benchmark::ClobberMemory();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Fnv1HashObjectId);

// --- parse_hex() - 6 bytes from string ---

static void ParseHex_6Bytes(benchmark::State &state) {
  const char *hex_str = "ABCDEF012345";
  uint8_t data[6];
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      parse_hex(hex_str, data, 6);
    }
    benchmark::DoNotOptimize(data);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ParseHex_6Bytes);

// --- parse_hex() - 16 bytes from string ---

static void ParseHex_16Bytes(benchmark::State &state) {
  const char *hex_str = "ABCDEF0123456789FEDCBA9876543210";
  uint8_t data[16];
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      parse_hex(hex_str, data, 16);
    }
    benchmark::DoNotOptimize(data);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ParseHex_16Bytes);

// --- crc8() - 8 bytes ---

static void CRC8_8Bytes(benchmark::State &state) {
  const uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  for (auto _ : state) {
    uint8_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result ^= crc8(data, 8);
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CRC8_8Bytes);

// --- crc16() - 8 bytes ---

static void CRC16_8Bytes(benchmark::State &state) {
  const uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  for (auto _ : state) {
    uint16_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result ^= crc16(data, 8);
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CRC16_8Bytes);

// --- value_accuracy_to_buf() - typical sensor value ---

static void ValueAccuracyToBuf(benchmark::State &state) {
  char raw_buf[VALUE_ACCURACY_MAX_LEN] = {};
  std::span<char, VALUE_ACCURACY_MAX_LEN> buf(raw_buf);
  float value = 23.456f;
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      value_accuracy_to_buf(buf, value, 2);
    }
    benchmark::DoNotOptimize(raw_buf);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(ValueAccuracyToBuf);

// --- int8_to_str() ---

static void Int8ToStr(benchmark::State &state) {
  char buffer[5] = {};
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      int8_to_str(buffer, static_cast<int8_t>(i & 0xFF));
      benchmark::DoNotOptimize(buffer);
      benchmark::ClobberMemory();
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Int8ToStr);

// --- base64_decode() - into pre-allocated buffer ---

static void Base64Decode_32Bytes(benchmark::State &state) {
  // 32 bytes encoded = 44 base64 chars
  const uint8_t encoded[] = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGx0eHw==";
  size_t encoded_len = 44;
  uint8_t output[32];
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      base64_decode(encoded, encoded_len, output, sizeof(output));
    }
    benchmark::DoNotOptimize(output);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Base64Decode_32Bytes);

// --- uint32_to_str() vs snprintf ---

static void Uint32ToStr_Small(benchmark::State &state) {
  char buf[UINT32_MAX_STR_SIZE];
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uint32_to_str(buf, 12345);
      benchmark::DoNotOptimize(buf);
      benchmark::ClobberMemory();
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Uint32ToStr_Small);

static void Snprintf_Uint32_Small(benchmark::State &state) {
  char buf[UINT32_MAX_STR_SIZE];
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      snprintf(buf, sizeof(buf), "%" PRIu32, static_cast<uint32_t>(12345));
      benchmark::DoNotOptimize(buf);
      benchmark::ClobberMemory();
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Snprintf_Uint32_Small);

static void Uint32ToStr_Large(benchmark::State &state) {
  char buf[UINT32_MAX_STR_SIZE];
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      uint32_to_str(buf, 4294967295u);
      benchmark::DoNotOptimize(buf);
      benchmark::ClobberMemory();
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Uint32ToStr_Large);

static void Snprintf_Uint32_Large(benchmark::State &state) {
  char buf[UINT32_MAX_STR_SIZE];
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      snprintf(buf, sizeof(buf), "%" PRIu32, static_cast<uint32_t>(4294967295u));
      benchmark::DoNotOptimize(buf);
      benchmark::ClobberMemory();
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Snprintf_Uint32_Large);

}  // namespace esphome::benchmarks
