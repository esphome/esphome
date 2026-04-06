#include <benchmark/benchmark.h>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_buffer.h"
#include "esphome/components/light/color_mode.h"

namespace esphome::api::benchmarks {

static constexpr int kInnerIterations = 2000;

// --- ListEntitiesSensorResponse ---

static ListEntitiesSensorResponse make_sensor_response() {
  ListEntitiesSensorResponse msg;
  msg.object_id = StringRef::from_lit("living_room_temperature");
  msg.key = 0x12345678;
  msg.name = StringRef::from_lit("Living Room Temperature");
#ifdef USE_ENTITY_ICON
  msg.icon = StringRef::from_lit("mdi:thermometer");
#endif
  msg.entity_category = enums::ENTITY_CATEGORY_NONE;
  msg.disabled_by_default = false;
  msg.unit_of_measurement = StringRef::from_lit("°C");
  msg.accuracy_decimals = 1;
  msg.force_update = false;
  msg.device_class = StringRef::from_lit("temperature");
  msg.state_class = enums::STATE_CLASS_MEASUREMENT;
#ifdef USE_DEVICES
  msg.device_id = 1;
#endif
  return msg;
}

static void CalculateSize_ListEntitiesSensorResponse(benchmark::State &state) {
  auto msg = make_sensor_response();

  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += msg.calculate_size();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CalculateSize_ListEntitiesSensorResponse);

static void Encode_ListEntitiesSensorResponse(benchmark::State &state) {
  auto msg = make_sensor_response();
  APIBuffer buffer;
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
BENCHMARK(Encode_ListEntitiesSensorResponse);

static void CalcAndEncode_ListEntitiesSensorResponse(benchmark::State &state) {
  auto msg = make_sensor_response();
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
BENCHMARK(CalcAndEncode_ListEntitiesSensorResponse);

// --- ListEntitiesBinarySensorResponse ---

static ListEntitiesBinarySensorResponse make_binary_sensor_response() {
  ListEntitiesBinarySensorResponse msg;
  msg.object_id = StringRef::from_lit("front_door_contact");
  msg.key = 0xAABBCCDD;
  msg.name = StringRef::from_lit("Front Door Contact");
#ifdef USE_ENTITY_ICON
  msg.icon = StringRef::from_lit("mdi:door");
#endif
  msg.entity_category = enums::ENTITY_CATEGORY_NONE;
  msg.disabled_by_default = false;
  msg.device_class = StringRef::from_lit("door");
  msg.is_status_binary_sensor = false;
#ifdef USE_DEVICES
  msg.device_id = 2;
#endif
  return msg;
}

static void CalculateSize_ListEntitiesBinarySensorResponse(benchmark::State &state) {
  auto msg = make_binary_sensor_response();

  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += msg.calculate_size();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CalculateSize_ListEntitiesBinarySensorResponse);

static void Encode_ListEntitiesBinarySensorResponse(benchmark::State &state) {
  auto msg = make_binary_sensor_response();
  APIBuffer buffer;
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
BENCHMARK(Encode_ListEntitiesBinarySensorResponse);

static void CalcAndEncode_ListEntitiesBinarySensorResponse(benchmark::State &state) {
  auto msg = make_binary_sensor_response();
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
BENCHMARK(CalcAndEncode_ListEntitiesBinarySensorResponse);

// --- ListEntitiesLightResponse ---

static light::ColorModeMask light_color_modes;
static FixedVector<const char *> light_effects;

static ListEntitiesLightResponse make_light_response() {
  // Initialize static data on first call
  static bool initialized = false;
  if (!initialized) {
    light_color_modes.insert(light::ColorMode::RGB_WHITE);
    light_color_modes.insert(light::ColorMode::COLOR_TEMPERATURE);
    light_effects.init(3);
    light_effects.push_back("None");
    light_effects.push_back("Rainbow");
    light_effects.push_back("Strobe");
    initialized = true;
  }

  ListEntitiesLightResponse msg;
  msg.object_id = StringRef::from_lit("kitchen_ceiling_light");
  msg.key = 0x55667788;
  msg.name = StringRef::from_lit("Kitchen Ceiling Light");
#ifdef USE_ENTITY_ICON
  msg.icon = StringRef::from_lit("mdi:ceiling-light");
#endif
  msg.entity_category = enums::ENTITY_CATEGORY_NONE;
  msg.disabled_by_default = false;
  msg.supported_color_modes = &light_color_modes;
  msg.min_mireds = 153.0f;
  msg.max_mireds = 500.0f;
  msg.effects = &light_effects;
#ifdef USE_DEVICES
  msg.device_id = 3;
#endif
  return msg;
}

static void CalculateSize_ListEntitiesLightResponse(benchmark::State &state) {
  auto msg = make_light_response();

  for (auto _ : state) {
    uint32_t result = 0;
    for (int i = 0; i < kInnerIterations; i++) {
      result += msg.calculate_size();
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CalculateSize_ListEntitiesLightResponse);

static void Encode_ListEntitiesLightResponse(benchmark::State &state) {
  auto msg = make_light_response();
  APIBuffer buffer;
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
BENCHMARK(Encode_ListEntitiesLightResponse);

static void CalcAndEncode_ListEntitiesLightResponse(benchmark::State &state) {
  auto msg = make_light_response();
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
BENCHMARK(CalcAndEncode_ListEntitiesLightResponse);

}  // namespace esphome::api::benchmarks
