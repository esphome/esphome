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

// --- BluetoothLERawAdvertisementsResponse (12 adverts, highest-volume BLE message) ---

#ifdef USE_BLUETOOTH_PROXY

static BluetoothLERawAdvertisementsResponse make_ble_raw_advs_12() {
  static const uint8_t fake_adv_data[] = {
      0x02, 0x01, 0x06, 0x03, 0x03, 0x9F, 0xFE, 0x17, 0x16, 0x9F, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  BluetoothLERawAdvertisementsResponse msg;
  msg.advertisements_len = 12;
  for (int i = 0; i < 12; i++) {
    auto &adv = msg.advertisements[i];
    adv.address = 0xAABBCCDD0000ULL + i;
    adv.rssi = -60 - i;
    adv.address_type = 1;
    memcpy(adv.data, fake_adv_data, sizeof(fake_adv_data));
    adv.data_len = sizeof(fake_adv_data);
  }
  return msg;
}

static void CalculateSize_BLERawAdvs12(benchmark::State &state) {
  auto msg = make_ble_raw_advs_12();

  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += msg.calculate_size();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CalculateSize_BLERawAdvs12);

static void Encode_BLERawAdvs12(benchmark::State &state) {
  auto msg = make_ble_raw_advs_12();
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
BENCHMARK(Encode_BLERawAdvs12);

static void CalcAndEncode_BLERawAdvs12(benchmark::State &state) {
  auto msg = make_ble_raw_advs_12();
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
BENCHMARK(CalcAndEncode_BLERawAdvs12);

static void CalcAndEncode_BLERawAdvs12_Fresh(benchmark::State &state) {
  auto msg = make_ble_raw_advs_12();

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
BENCHMARK(CalcAndEncode_BLERawAdvs12_Fresh);

#endif  // USE_BLUETOOTH_PROXY

// --- ZWaveProxyFrame (Z-Wave frame, ~16 bytes payload) ---

#ifdef USE_ZWAVE_PROXY

static constexpr uint8_t kZWaveFrameData[] = {0x01, 0x09, 0x00, 0x13, 0x01, 0x02, 0x00, 0x00,
                                              0x25, 0x00, 0x05, 0xC4, 0x00, 0x00, 0x00, 0x00};

static ZWaveProxyFrame make_zwave_proxy_frame() {
  ZWaveProxyFrame msg;
  msg.data = kZWaveFrameData;
  msg.data_len = sizeof(kZWaveFrameData);
  return msg;
}

static void Encode_ZWaveProxyFrame(benchmark::State &state) {
  auto msg = make_zwave_proxy_frame();
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

#endif  // USE_ZWAVE_PROXY

// --- SerialProxyDataReceived (serial passthrough, 64-byte payload) ---

#ifdef USE_SERIAL_PROXY

static constexpr size_t kSerialPayloadSize = 64;
static const uint8_t kSerialPayload[kSerialPayloadSize] = {
    0x55, 0xAA, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB,
    0xCD, 0xEF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
    0xFF, 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0,
    0xF0, 0x0F, 0x1F, 0x2F, 0x3F, 0x4F, 0x5F, 0x6F, 0x7F, 0x8F, 0x9F, 0xAF, 0xBF, 0xCF, 0xDF, 0xEF};

static SerialProxyDataReceived make_serial_proxy_data_received() {
  SerialProxyDataReceived msg;
  msg.instance = 0;
  msg.set_data(kSerialPayload, kSerialPayloadSize);
  return msg;
}

static void Encode_SerialProxyDataReceived(benchmark::State &state) {
  auto msg = make_serial_proxy_data_received();
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

#endif  // USE_SERIAL_PROXY

// --- InfraredRFReceiveEvent (100 timings, typical IR/RF capture) ---

#if defined(USE_IR_RF) || defined(USE_RADIO_FREQUENCY)

// Mark/space pairs simulating a typical RC-5 / NEC capture (100 timings).
static const std::vector<int32_t> &get_ir_timings_100() {
  static std::vector<int32_t> *timings = nullptr;
  if (timings == nullptr) {
    timings = new std::vector<int32_t>();
    timings->reserve(100);
    for (int i = 0; i < 100; i++) {
      timings->push_back((i % 2 == 0) ? 560 : -560);
    }
  }
  return *timings;
}

static InfraredRFReceiveEvent make_infrared_rf_receive_event() {
  InfraredRFReceiveEvent msg;
  msg.key = 0xDEADBEEF;
  msg.timings = &get_ir_timings_100();
  return msg;
}

static void Encode_InfraredRFReceiveEvent(benchmark::State &state) {
  auto msg = make_infrared_rf_receive_event();
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
  auto msg = make_infrared_rf_receive_event();

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

#endif  // USE_IR_RF || USE_RADIO_FREQUENCY

}  // namespace esphome::api::benchmarks
