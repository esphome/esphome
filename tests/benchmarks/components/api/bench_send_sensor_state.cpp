#include "esphome/core/defines.h"
#if defined(USE_API_PLAINTEXT) && defined(USE_SENSOR)

#include <benchmark/benchmark.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::api {

// Friend function declared in APIConnection to enable immediate send path for benchmarking.
void bench_enable_immediate_send(APIConnection *conn) { conn->flags_.should_try_send_immediately = true; }

}  // namespace esphome::api

namespace esphome::api::benchmarks {

static constexpr int kInnerIterations = 2000;

// Helper to drain accumulated data from the read side of a socket
// to prevent the write side from blocking.
static void drain_socket(int fd) {
  char buf[65536];
  while (::read(fd, buf, sizeof(buf)) > 0) {
  }
}

// Helper to create a TCP loopback connection with an APIConnection.
// Returns the connection and the read-side fd for draining.
static std::pair<std::unique_ptr<APIConnection>, int> create_api_connection() {
  // Create a TCP listener on loopback
  int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;  // OS-assigned port
  ::bind(listen_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
  ::listen(listen_fd, 1);

  // Get the assigned port
  socklen_t addr_len = sizeof(addr);
  ::getsockname(listen_fd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len);

  // Connect from client side
  int write_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  ::connect(write_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));

  // Accept on server side (this is our read fd)
  int read_fd = ::accept(listen_fd, nullptr, nullptr);
  ::close(listen_fd);

  // Make both ends non-blocking
  int flags = ::fcntl(write_fd, F_GETFL, 0);
  ::fcntl(write_fd, F_SETFL, flags | O_NONBLOCK);
  flags = ::fcntl(read_fd, F_GETFL, 0);
  ::fcntl(read_fd, F_SETFL, flags | O_NONBLOCK);

  // Increase socket buffer sizes to reduce drain frequency
  int bufsize = 1024 * 1024;
  ::setsockopt(write_fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
  ::setsockopt(read_fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));

  auto sock = std::make_unique<socket::Socket>(write_fd);
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

// --- send_sensor_state: batch path (default for new connections) ---
// Measures: send_message_smart_ → schedule_message_ → deferred batch add.
// This is the default path until initial states are sent.

static void SendSensorState_Batch(benchmark::State &state) {
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
BENCHMARK(SendSensorState_Batch);

}  // namespace esphome::api::benchmarks

#endif  // USE_API_PLAINTEXT && USE_SENSOR
