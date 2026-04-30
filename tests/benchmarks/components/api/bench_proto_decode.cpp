#include <benchmark/benchmark.h>

#include <cstring>

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

// --- ZWaveProxyFrame decode (~16-byte data buffer) ---

#ifdef USE_ZWAVE_PROXY

static void Decode_ZWaveProxyFrame(benchmark::State &state) {
  static const uint8_t frame_data[] = {0x01, 0x09, 0x00, 0x13, 0x01, 0x02, 0x00, 0x00,
                                       0x25, 0x00, 0x05, 0xC4, 0x00, 0x00, 0x00, 0x00};
  ZWaveProxyFrame source;
  source.data = frame_data;
  source.data_len = sizeof(frame_data);
  auto encoded = encode_message(source);
  auto *data = encoded.data();
  auto size = encoded.size();
  benchmark::DoNotOptimize(data);
  benchmark::DoNotOptimize(size);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ZWaveProxyFrame msg;
      escape(&msg);
      msg.decode(data, size);
      escape(&msg);
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_ZWaveProxyFrame);

static void Decode_ZWaveProxyRequest(benchmark::State &state) {
  static const uint8_t req_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
  ZWaveProxyRequest source;
  source.type = enums::ZWAVE_PROXY_REQUEST_TYPE_HOME_ID_CHANGE;
  source.data = req_data;
  source.data_len = sizeof(req_data);
  auto encoded = encode_message(source);
  auto *data = encoded.data();
  auto size = encoded.size();
  benchmark::DoNotOptimize(data);
  benchmark::DoNotOptimize(size);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ZWaveProxyRequest msg;
      escape(&msg);
      msg.decode(data, size);
      escape(&msg);
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_ZWaveProxyRequest);

#endif  // USE_ZWAVE_PROXY

// --- SerialProxyWriteRequest decode (instance + 64-byte data) ---
//
// SerialProxyWriteRequest is decode-only (SOURCE_CLIENT), so we encode via
// SerialProxyDataReceived which has identical wire format
// (uint32 instance = 1; bytes data = 2;).

#ifdef USE_SERIAL_PROXY

static void Decode_SerialProxyWriteRequest(benchmark::State &state) {
  static constexpr size_t kPayloadSize = 64;
  static uint8_t payload[kPayloadSize];
  for (size_t i = 0; i < kPayloadSize; i++)
    payload[i] = static_cast<uint8_t>(i);

  SerialProxyDataReceived source;
  source.instance = 0;
  source.set_data(payload, kPayloadSize);
  auto encoded = encode_message(source);
  auto *data = encoded.data();
  auto size = encoded.size();
  benchmark::DoNotOptimize(data);
  benchmark::DoNotOptimize(size);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      SerialProxyWriteRequest msg;
      escape(&msg);
      msg.decode(data, size);
      escape(&msg);
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_SerialProxyWriteRequest);

#endif  // USE_SERIAL_PROXY

// --- InfraredRFTransmitRawTimingsRequest decode (100 zigzag-encoded timings) ---
//
// Hand-built wire bytes since this message is decode-only and has no sister
// type with an identical layout. Wire format:
//   field 2 (key, fixed32):           tag=0x15, 4 LE bytes
//   field 3 (carrier_frequency):      tag=0x18, varint
//   field 4 (repeat_count):           tag=0x20, varint
//   field 5 (timings, packed sint32): tag=0x2A, length varint, packed payload
//   field 6 (modulation):             tag=0x30, varint

#if defined(USE_IR_RF) || defined(USE_RADIO_FREQUENCY)

static APIBuffer build_infrared_rf_transmit_wire() {
  // Build the entire wire payload into a stack buffer, then copy into the
  // returned APIBuffer in a single resize+memcpy. Keeps allocation count
  // low so callgrind/valgrind doesn't churn through hundreds of grow_()s.
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

  // field 2: key (fixed32) = 0xDEADBEEF
  put_byte(0x15);
  put_byte(0xEF);
  put_byte(0xBE);
  put_byte(0xAD);
  put_byte(0xDE);
  // field 3: carrier_frequency = 38000
  put_byte(0x18);
  put_varint(38000);
  // field 4: repeat_count = 2
  put_byte(0x20);
  put_varint(2);
  // field 5: timings (packed sint32) — 100 entries alternating mark/space.
  // Each entry encodes to 2 bytes (zigzag(560)=1120 → varint 0xE0 0x08), so
  // packed payload is 200 bytes; with tag (1) + length varint (2) it fits in
  // the 256-byte stack buffer.
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
  // field 6: modulation = 0 — skip (default value, not encoded by senders)

  APIBuffer buf;
  buf.resize(len);
  std::memcpy(buf.data(), bytes, len);
  return buf;
}

static void Decode_InfraredRFTransmitRawTimingsRequest(benchmark::State &state) {
  auto encoded = build_infrared_rf_transmit_wire();
  auto *data = encoded.data();
  auto size = encoded.size();
  benchmark::DoNotOptimize(data);
  benchmark::DoNotOptimize(size);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      InfraredRFTransmitRawTimingsRequest msg;
      escape(&msg);
      msg.decode(data, size);
      escape(&msg);
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Decode_InfraredRFTransmitRawTimingsRequest);

#endif  // USE_IR_RF || USE_RADIO_FREQUENCY

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
