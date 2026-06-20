// Encode/decode microbenchmarks for proxy message families that carry
// high-volume traffic (Z-Wave, IR/RF, serial). Mirrors the existing
// BluetoothLERawAdvertisementsResponse benchmarks in bench_proto_encode.cpp.

#include <benchmark/benchmark.h>

#include <cstring>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_buffer.h"

namespace esphome::api::benchmarks {

static constexpr int kInnerIterations = 2000;

// Encodes `src` into `out`. Caller owns `out` and must keep it alive across
// the decode loop (decoded messages may store pointers back into its bytes).
template<typename T> static void encode_into(APIBuffer &out, const T &src) {
  out.resize(src.calculate_size());
  ProtoWriteBuffer writer(&out, 0);
  src.encode(writer);
}

// --- ZWaveProxyFrame (Z-Wave frame, ~16 bytes payload) ---

#ifdef USE_ZWAVE_PROXY

static const uint8_t kZWaveFrameData[] = {0x01, 0x09, 0x00, 0x13, 0x01, 0x02, 0x00, 0x00,
                                          0x25, 0x00, 0x05, 0xC4, 0x00, 0x00, 0x00, 0x00};

static void Encode_ZWaveProxyFrame(benchmark::State &state) {
  ZWaveProxyFrame msg;
  msg.data = kZWaveFrameData;
  msg.data_len = sizeof(kZWaveFrameData);
  APIBuffer buffer;
  buffer.resize(msg.calculate_size());

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ProtoWriteBuffer writer(&buffer, 0);
      msg.encode(writer);
    }
    benchmark::DoNotOptimize(buffer.data());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Encode_ZWaveProxyFrame);

static void Decode_ZWaveProxyFrame(benchmark::State &state) {
  ZWaveProxyFrame source;
  source.data = kZWaveFrameData;
  source.data_len = sizeof(kZWaveFrameData);
  APIBuffer encoded;
  encode_into(encoded, source);
  const uint8_t *data = encoded.data();
  size_t size = encoded.size();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ZWaveProxyFrame msg;
      msg.decode(data, size);
      benchmark::DoNotOptimize(msg);
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_ZWaveProxyFrame);

static const uint8_t kZWaveRequestData[] = {0xDE, 0xAD, 0xBE, 0xEF};

static void Decode_ZWaveProxyRequest(benchmark::State &state) {
  ZWaveProxyRequest source;
  source.type = enums::ZWAVE_PROXY_REQUEST_TYPE_HOME_ID_CHANGE;
  source.data = kZWaveRequestData;
  source.data_len = sizeof(kZWaveRequestData);
  APIBuffer encoded;
  encode_into(encoded, source);
  const uint8_t *data = encoded.data();
  size_t size = encoded.size();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ZWaveProxyRequest msg;
      msg.decode(data, size);
      benchmark::DoNotOptimize(msg);
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_ZWaveProxyRequest);

#endif  // USE_ZWAVE_PROXY

// --- SerialProxyDataReceived encode + SerialProxyWriteRequest decode ---
//
// SerialProxyWriteRequest is decode-only (SOURCE_CLIENT) but has the same
// wire layout as SerialProxyDataReceived, so we encode via the latter and
// decode as the former.

#ifdef USE_SERIAL_PROXY

static constexpr size_t kSerialPayloadSize = 64;
static const uint8_t kSerialPayload[kSerialPayloadSize] = {
    0x55, 0xAA, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB,
    0xCD, 0xEF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
    0xFF, 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0,
    0xF0, 0x0F, 0x1F, 0x2F, 0x3F, 0x4F, 0x5F, 0x6F, 0x7F, 0x8F, 0x9F, 0xAF, 0xBF, 0xCF, 0xDF, 0xEF};

static void Encode_SerialProxyDataReceived(benchmark::State &state) {
  SerialProxyDataReceived msg;
  msg.instance = 0;
  msg.set_data(kSerialPayload, kSerialPayloadSize);
  APIBuffer buffer;
  buffer.resize(msg.calculate_size());

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ProtoWriteBuffer writer(&buffer, 0);
      msg.encode(writer);
    }
    benchmark::DoNotOptimize(buffer.data());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Encode_SerialProxyDataReceived);

static void Decode_SerialProxyWriteRequest(benchmark::State &state) {
  SerialProxyDataReceived source;
  source.instance = 0;
  source.set_data(kSerialPayload, kSerialPayloadSize);
  APIBuffer encoded;
  encode_into(encoded, source);
  const uint8_t *data = encoded.data();
  size_t size = encoded.size();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      SerialProxyWriteRequest msg;
      msg.decode(data, size);
      benchmark::DoNotOptimize(msg);
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_SerialProxyWriteRequest);

#endif  // USE_SERIAL_PROXY

// --- InfraredRFReceiveEvent encode (100 sint32 timings) +
//     InfraredRFTransmitRawTimingsRequest decode (hand-built wire bytes) ---

#if defined(USE_IR_RF) || defined(USE_RADIO_FREQUENCY)

// Mark/space pairs simulating a typical RC-5 / NEC capture (100 timings).
static std::vector<int32_t> make_ir_timings_100() {
  std::vector<int32_t> v;
  v.reserve(100);
  for (int i = 0; i < 100; i++) {
    v.push_back((i % 2 == 0) ? 560 : -560);
  }
  return v;
}

static const std::vector<int32_t> &get_ir_timings_100() {
  static const std::vector<int32_t> timings = make_ir_timings_100();
  return timings;
}

static void Encode_InfraredRFReceiveEvent(benchmark::State &state) {
  InfraredRFReceiveEvent msg;
  msg.key = 0xDEADBEEF;
  msg.timings = &get_ir_timings_100();
  APIBuffer buffer;
  buffer.resize(msg.calculate_size());

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ProtoWriteBuffer writer(&buffer, 0);
      msg.encode(writer);
    }
    benchmark::DoNotOptimize(buffer.data());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Encode_InfraredRFReceiveEvent);

static void CalculateSize_InfraredRFReceiveEvent(benchmark::State &state) {
  InfraredRFReceiveEvent msg;
  msg.key = 0xDEADBEEF;
  msg.timings = &get_ir_timings_100();

  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += msg.calculate_size();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CalculateSize_InfraredRFReceiveEvent);

// Hand-built wire bytes for InfraredRFTransmitRawTimingsRequest (decode-only,
// no sister message with identical wire layout).
//   field 2 (key, fixed32):           tag=0x15, 4 LE bytes
//   field 3 (carrier_frequency):      tag=0x18, varint
//   field 4 (repeat_count):           tag=0x20, varint
//   field 5 (timings, packed sint32): tag=0x2A, length varint, packed payload
//   field 6 (modulation):             tag=0x30, varint
static APIBuffer build_infrared_rf_transmit_wire() {
  uint8_t bytes[256];
  size_t len = 0;

  auto put_byte = [&](uint8_t b) { bytes[len++] = b; };
  auto put_varint = [&](uint32_t v) {
    while (v >= 0x80) {
      bytes[len++] = static_cast<uint8_t>((v & 0x7F) | 0x80);
      v >>= 7;
    }
    bytes[len++] = static_cast<uint8_t>(v);
  };
  auto encode_zigzag = [](int32_t v) -> uint32_t {
    return (static_cast<uint32_t>(v) << 1) ^ static_cast<uint32_t>(v >> 31);
  };

  put_byte(0x15);
  put_byte(0xEF);
  put_byte(0xBE);
  put_byte(0xAD);
  put_byte(0xDE);
  put_byte(0x18);
  put_varint(38000);
  put_byte(0x20);
  put_varint(2);

  uint8_t packed[200];
  size_t packed_len = 0;
  for (int i = 0; i < 100; i++) {
    int32_t value = (i % 2 == 0) ? 560 : -560;
    uint32_t zz = encode_zigzag(value);
    while (zz >= 0x80) {
      packed[packed_len++] = static_cast<uint8_t>((zz & 0x7F) | 0x80);
      zz >>= 7;
    }
    packed[packed_len++] = static_cast<uint8_t>(zz);
  }
  put_byte(0x2A);
  put_varint(static_cast<uint32_t>(packed_len));
  std::memcpy(bytes + len, packed, packed_len);
  len += packed_len;
  // field 6: modulation = 1 (non-zero so it's actually emitted and exercises
  // decode_varint for this field, matching the documented layout above).
  put_byte(0x30);
  put_varint(1);

  APIBuffer buf;
  buf.resize(len);
  std::memcpy(buf.data(), bytes, len);
  return buf;
}

static void Decode_InfraredRFTransmitRawTimingsRequest(benchmark::State &state) {
  auto encoded = build_infrared_rf_transmit_wire();
  const uint8_t *data = encoded.data();
  size_t size = encoded.size();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      InfraredRFTransmitRawTimingsRequest msg;
      msg.decode(data, size);
      benchmark::DoNotOptimize(msg);
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_InfraredRFTransmitRawTimingsRequest);

#endif  // USE_IR_RF || USE_RADIO_FREQUENCY

}  // namespace esphome::api::benchmarks
