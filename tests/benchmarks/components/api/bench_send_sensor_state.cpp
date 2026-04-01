#include "esphome/core/defines.h"
#if defined(USE_API_PLAINTEXT) && defined(USE_SENSOR)

#include <benchmark/benchmark.h>
#include <unistd.h>

#include "bench_helpers.h"
#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::api {

// Friend functions declared in APIConnection for benchmark access.
void bench_enable_immediate_send(APIConnection *conn) { conn->flags_.should_try_send_immediately = true; }
void bench_clear_batch(APIConnection *conn) { conn->clear_batch_(); }

}  // namespace esphome::api

namespace esphome::api::benchmarks {

static constexpr int kInnerIterations = 2000;

// Helper to create a TCP loopback connection with an APIConnection.
// Returns the connection and the read-side fd for draining.
static std::pair<std::unique_ptr<APIConnection>, int> create_api_connection() {
  auto [sock, read_fd] = create_tcp_loopback();
  auto conn = std::make_unique<APIConnection>(std::move(sock), global_api_server);
  conn->start();
  return {std::move(conn), read_fd};
}

// Test subclass to access protected configure_entity_() for benchmark setup.
class TestSensor : public sensor::Sensor {
 public:
  void configure(const char *name) { this->configure_entity_(name, 0x12345678, 0); }
};

// --- send_sensor_state: immediate send path ---
// Measures: send_message_smart_ → prepare buffer → dispatch_message_ →
// try_send_sensor_state → fill key/device_id + proto encode → frame write →
// TCP send. This is the per-client cost when batch_delay=0 and initial states
// have been sent.

static void SendSensorState_Immediate(benchmark::State &state) {
  auto [conn, read_fd] = create_api_connection();
  bench_enable_immediate_send(conn.get());

  TestSensor sensor;
  sensor.configure("test_sensor");
  sensor.publish_state(23.5f);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      conn->send_sensor_state(&sensor);

      if ((i & 0xFF) == 0)
        drain_socket(read_fd);
    }
    drain_socket(read_fd);
    benchmark::DoNotOptimize(conn.get());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);

  ::close(read_fd);
}
BENCHMARK(SendSensorState_Immediate);

// --- send_sensor_state: batch path (cold — first call allocates) ---
// Measures: send_message_smart_ → schedule_message_ → deferred batch add.
// Includes one-time vector allocation cost.

static void SendSensorState_Batch_Cold(benchmark::State &state) {
  auto [conn, read_fd] = create_api_connection();

  TestSensor sensor;
  sensor.configure("test_sensor");
  sensor.publish_state(23.5f);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      conn->send_sensor_state(&sensor);
    }
    benchmark::DoNotOptimize(conn.get());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);

  ::close(read_fd);
}
BENCHMARK(SendSensorState_Batch_Cold);

// --- send_sensor_state: batch path (warm — buffer already allocated) ---
// Measures steady-state batch cost after the vector has been allocated
// and cleared at least once. This is the typical path during normal
// operation after the first batch has been processed.

static void SendSensorState_Batch_Warm(benchmark::State &state) {
  auto [conn, read_fd] = create_api_connection();

  TestSensor sensor;
  sensor.configure("test_sensor");
  sensor.publish_state(23.5f);

  // Warm up: send once to allocate, then clear to keep capacity
  conn->send_sensor_state(&sensor);
  bench_clear_batch(conn.get());

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      conn->send_sensor_state(&sensor);
    }
    benchmark::DoNotOptimize(conn.get());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);

  ::close(read_fd);
}
BENCHMARK(SendSensorState_Batch_Warm);

}  // namespace esphome::api::benchmarks

#endif  // USE_API_PLAINTEXT && USE_SENSOR
