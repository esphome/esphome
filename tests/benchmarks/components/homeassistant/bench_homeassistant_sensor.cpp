#include "esphome/core/defines.h"
#if defined(USE_API_PLAINTEXT) && defined(USE_API_HOMEASSISTANT_STATES) && defined(USE_SENSOR)

#include <benchmark/benchmark.h>
#include <sys/socket.h>

#include <array>
#include <memory>

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/homeassistant/sensor/homeassistant_sensor.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant::benchmarks {

static constexpr int kInnerIterations = 2000;

// Each benchmark gets its own API server so subscriptions from one do not
// stay in the list scanned by the next. The constructor installs itself as
// global_api_server, which is what HomeassistantSensor::setup() subscribes to.
class ScopedApiServer {
 public:
  ScopedApiServer() : previous_(api::global_api_server) {}
  ~ScopedApiServer() { api::global_api_server = this->previous_; }
  api::APIServer *get() { return &this->server_; }

 protected:
  api::APIServer *previous_;
  api::APIServer server_;
};

// The handler only reads the server's subscription list, so the connection is
// never started and its socket never used.
static std::unique_ptr<api::APIConnection> create_connection(api::APIServer *server) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  return std::make_unique<api::APIConnection>(std::make_unique<socket::Socket>(fd), server);
}

// State strings as Home Assistant sends them. The handler writes a terminator
// one byte past the state, so the buffers are mutable and the terminator it
// writes lands on the one the literal already has.
static constexpr int kStateCount = 4;
static constexpr size_t kStateBufferSize = 16;
using StateBuffers = char[kStateCount][kStateBufferSize];
using Messages = std::array<api::HomeAssistantStateResponse, kStateCount>;

static StateBuffers NUMERIC_STATES = {"100.50", "101.25", "99.75", "100.00"};
static StateBuffers INTEGER_STATES = {"100", "101", "99", "0"};
static StateBuffers UNAVAILABLE_STATES = {"unavailable", "unknown", "unavailable", "unknown"};

static Messages make_messages(const char *entity_id, const char *attribute, StateBuffers &states) {
  Messages msgs;
  for (int i = 0; i < kStateCount; i++) {
    msgs[i].entity_id = StringRef(entity_id);
    if (attribute != nullptr) {
      msgs[i].attribute = StringRef(attribute);
    }
    msgs[i].state = StringRef(states[i]);
  }
  return msgs;
}

// --- One imported sensor, numeric state that changes every message ---
// Exercises: subscription match, in-place terminator, parse_number, publish_state.

static void HomeassistantSensor_StateResponse(benchmark::State &state) {
  ScopedApiServer server;
  auto conn = create_connection(server.get());
  const Messages msgs = make_messages("sensor.power", nullptr, NUMERIC_STATES);

  HomeassistantSensor sensor;
  sensor.set_entity_id("sensor.power");
  sensor.setup();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      conn->on_home_assistant_state_response(msgs[i % kStateCount]);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(HomeassistantSensor_StateResponse);

// --- The same sensor with integer states such as a percentage or a count ---
// Exercises: the same path with no fractional digits to parse. Only the state
// differs from the decimal case, so the gap between the two is the parse.

static void HomeassistantSensor_StateResponse_Integer(benchmark::State &state) {
  ScopedApiServer server;
  auto conn = create_connection(server.get());
  const Messages msgs = make_messages("sensor.power", nullptr, INTEGER_STATES);

  HomeassistantSensor sensor;
  sensor.set_entity_id("sensor.power");
  sensor.setup();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      conn->on_home_assistant_state_response(msgs[i % kStateCount]);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(HomeassistantSensor_StateResponse_Integer);

// --- Attribute import: the subscription and the message both carry an attribute ---
// Exercises: the extra attribute compare before the callback.

static void HomeassistantSensor_StateResponse_Attribute(benchmark::State &state) {
  ScopedApiServer server;
  auto conn = create_connection(server.get());
  const Messages msgs = make_messages("climate.living_room", "current_temperature", NUMERIC_STATES);

  HomeassistantSensor sensor;
  sensor.set_entity_id("climate.living_room");
  sensor.set_attribute("current_temperature");
  sensor.setup();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      conn->on_home_assistant_state_response(msgs[i % kStateCount]);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(HomeassistantSensor_StateResponse_Attribute);

// --- Sixteen imported sensors, message for the last one ---
// Exercises: the linear scan over the subscription list with a string compare
// per entry, which is what a device with many imported entities pays per update.
// The IDs share a length and differ only in the last two bytes, so every
// entry runs a near-full compare, the worst case for the scan.

static constexpr int kSensorCount = 16;

static void HomeassistantSensor_StateResponse_ManySubscriptions(benchmark::State &state) {
  ScopedApiServer server;
  auto conn = create_connection(server.get());

  static constexpr const char *const ENTITY_IDS[kSensorCount] = {
      "sensor.import_00", "sensor.import_01", "sensor.import_02", "sensor.import_03",
      "sensor.import_04", "sensor.import_05", "sensor.import_06", "sensor.import_07",
      "sensor.import_08", "sensor.import_09", "sensor.import_10", "sensor.import_11",
      "sensor.import_12", "sensor.import_13", "sensor.import_14", "sensor.import_15",
  };
  HomeassistantSensor sensors[kSensorCount];
  for (int i = 0; i < kSensorCount; i++) {
    sensors[i].set_entity_id(ENTITY_IDS[i]);
    sensors[i].setup();
  }
  const Messages msgs = make_messages(ENTITY_IDS[kSensorCount - 1], nullptr, NUMERIC_STATES);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      conn->on_home_assistant_state_response(msgs[i % kStateCount]);
    }
    benchmark::DoNotOptimize(sensors[kSensorCount - 1].state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(HomeassistantSensor_StateResponse_ManySubscriptions);

// --- Non-numeric state such as "unavailable" ---
// Exercises: the failed parse, the warning the platform logs for it, and the
// NAN publish. The benchmark logger runs at the production default level with
// console output off, so a log line at DEBUG or above costs its formatting
// here, the same as on a device, and a log level change in the platform shows
// up as a delta.

static void HomeassistantSensor_StateResponse_Unavailable(benchmark::State &state) {
  ScopedApiServer server;
  auto conn = create_connection(server.get());
  const Messages msgs = make_messages("sensor.power", nullptr, UNAVAILABLE_STATES);

  HomeassistantSensor sensor;
  sensor.set_entity_id("sensor.power");
  sensor.setup();

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      conn->on_home_assistant_state_response(msgs[i % kStateCount]);
    }
    benchmark::DoNotOptimize(sensor.state);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(HomeassistantSensor_StateResponse_Unavailable);

}  // namespace esphome::homeassistant::benchmarks

#endif  // USE_API_PLAINTEXT && USE_API_HOMEASSISTANT_STATES && USE_SENSOR
