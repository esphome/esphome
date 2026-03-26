#include <benchmark/benchmark.h>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_buffer.h"

namespace esphome::api::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
// Without this, the ~60ns per-iteration valgrind start/stop cost dominates
// sub-microsecond benchmarks.
static constexpr int kInnerIterations = 2000;

// --- SensorStateResponse (highest frequency message) ---

static void Encode_SensorStateResponse(benchmark::State &state) {
  APIBuffer buffer;
  SensorStateResponse msg;
  msg.key = 0x12345678;
  msg.state = 23.5f;
  msg.missing_state = false;
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
BENCHMARK(Encode_SensorStateResponse);

static void CalculateSize_SensorStateResponse(benchmark::State &state) {
  SensorStateResponse msg;
  msg.key = 0x12345678;
  msg.state = 23.5f;
  msg.missing_state = false;

  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += msg.calculate_size();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CalculateSize_SensorStateResponse);

// Steady state: buffer already allocated from previous iteration
static void CalcAndEncode_SensorStateResponse(benchmark::State &state) {
  APIBuffer buffer;
  SensorStateResponse msg;
  msg.key = 0x12345678;
  msg.state = 23.5f;
  msg.missing_state = false;

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
BENCHMARK(CalcAndEncode_SensorStateResponse);

// Cold path: fresh buffer each iteration (measures heap allocation cost).
// Inner loop still needed to amortize CodSpeed instrumentation overhead.
// Each inner iteration creates a fresh buffer, so this measures
// alloc+calc+encode per item.
static void CalcAndEncode_SensorStateResponse_Fresh(benchmark::State &state) {
  SensorStateResponse msg;
  msg.key = 0x12345678;
  msg.state = 23.5f;
  msg.missing_state = false;

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
BENCHMARK(CalcAndEncode_SensorStateResponse_Fresh);

// --- BinarySensorStateResponse ---

static void Encode_BinarySensorStateResponse(benchmark::State &state) {
  APIBuffer buffer;
  BinarySensorStateResponse msg;
  msg.key = 0xAABBCCDD;
  msg.state = true;
  msg.missing_state = false;
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
BENCHMARK(Encode_BinarySensorStateResponse);

// --- HelloResponse (string fields) ---

static void Encode_HelloResponse(benchmark::State &state) {
  APIBuffer buffer;
  HelloResponse msg;
  msg.api_version_major = 1;
  msg.api_version_minor = 10;
  msg.server_info = StringRef::from_lit("esphome v2026.3.0");
  msg.name = StringRef::from_lit("living-room-sensor");
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
BENCHMARK(Encode_HelloResponse);

// --- LightStateResponse (complex multi-field message) ---

static void Encode_LightStateResponse(benchmark::State &state) {
  APIBuffer buffer;
  LightStateResponse msg;
  msg.key = 0x11223344;
  msg.state = true;
  msg.brightness = 0.8f;
  msg.color_mode = enums::COLOR_MODE_RGB_WHITE;
  msg.color_brightness = 1.0f;
  msg.red = 1.0f;
  msg.green = 0.5f;
  msg.blue = 0.2f;
  msg.white = 0.0f;
  msg.color_temperature = 4000.0f;
  msg.cold_white = 0.0f;
  msg.warm_white = 0.0f;
  msg.effect = StringRef::from_lit("rainbow");
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
BENCHMARK(Encode_LightStateResponse);

static void CalculateSize_LightStateResponse(benchmark::State &state) {
  LightStateResponse msg;
  msg.key = 0x11223344;
  msg.state = true;
  msg.brightness = 0.8f;
  msg.color_mode = enums::COLOR_MODE_RGB_WHITE;
  msg.color_brightness = 1.0f;
  msg.red = 1.0f;
  msg.green = 0.5f;
  msg.blue = 0.2f;
  msg.white = 0.0f;
  msg.color_temperature = 4000.0f;
  msg.cold_white = 0.0f;
  msg.warm_white = 0.0f;
  msg.effect = StringRef::from_lit("rainbow");

  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += msg.calculate_size();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CalculateSize_LightStateResponse);

// --- DeviceInfoResponse (nested submessages: 20 devices + 20 areas) ---

static DeviceInfoResponse make_device_info_response() {
  DeviceInfoResponse msg;
  msg.name = StringRef::from_lit("living-room-sensor");
  msg.mac_address = StringRef::from_lit("AA:BB:CC:DD:EE:FF");
  msg.esphome_version = StringRef::from_lit("2026.3.0");
  msg.compilation_time = StringRef::from_lit("Mar 16 2026, 12:00:00");
  msg.model = StringRef::from_lit("esp32-poe-iso");
  msg.manufacturer = StringRef::from_lit("Olimex");
  msg.friendly_name = StringRef::from_lit("Living Room Sensor");
#ifdef USE_DEVICES
  for (uint32_t i = 0; i < ESPHOME_DEVICE_COUNT && i < 20; i++) {
    msg.devices[i].device_id = i + 1;
    msg.devices[i].name = StringRef::from_lit("device");
    msg.devices[i].area_id = (i % 20) + 1;
  }
#endif
#ifdef USE_AREAS
  for (uint32_t i = 0; i < ESPHOME_AREA_COUNT && i < 20; i++) {
    msg.areas[i].area_id = i + 1;
    msg.areas[i].name = StringRef::from_lit("area");
  }
#endif
  return msg;
}

static void CalculateSize_DeviceInfoResponse(benchmark::State &state) {
  auto msg = make_device_info_response();

  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += msg.calculate_size();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CalculateSize_DeviceInfoResponse);

static void Encode_DeviceInfoResponse(benchmark::State &state) {
  auto msg = make_device_info_response();
  APIBuffer buffer;
  uint32_t total_size = msg.calculate_size();
  buffer.resize(total_size);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      ProtoWriteBuffer writer(&buffer, 0);
      msg.encode(writer);
    }
    benchmark::DoNotOptimize(buffer.data());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(Encode_DeviceInfoResponse);

// Steady state: buffer already allocated from previous iteration
static void CalcAndEncode_DeviceInfoResponse(benchmark::State &state) {
  auto msg = make_device_info_response();
  APIBuffer buffer;

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
BENCHMARK(CalcAndEncode_DeviceInfoResponse);

// Cold path: fresh buffer each iteration (measures heap allocation cost).
// Inner loop still needed to amortize CodSpeed instrumentation overhead.
// Each inner iteration creates a fresh buffer, so this measures
// alloc+calc+encode per item.
static void CalcAndEncode_DeviceInfoResponse_Fresh(benchmark::State &state) {
  auto msg = make_device_info_response();

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
BENCHMARK(CalcAndEncode_DeviceInfoResponse_Fresh);

}  // namespace esphome::api::benchmarks
