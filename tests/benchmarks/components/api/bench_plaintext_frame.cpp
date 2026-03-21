#include "esphome/core/defines.h"
#ifdef USE_API_PLAINTEXT

#include <benchmark/benchmark.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/api/api_frame_helper_plaintext.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_buffer.h"

namespace esphome::api::benchmarks {

static constexpr int kInnerIterations = 2000;

// Helper to drain accumulated data from the read side of a socket
// to prevent the write side from blocking.
static void drain_socket(int fd) {
  char buf[65536];
  while (::read(fd, buf, sizeof(buf)) > 0) {
  }
}

// Helper to create a TCP loopback connection with an APIPlaintextFrameHelper
// on the write end. Returns the helper and the read-side fd.
// Uses real TCP sockets so TCP_NODELAY succeeds during init().
static std::pair<std::unique_ptr<APIPlaintextFrameHelper>, int> create_plaintext_helper() {
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
  auto helper = std::make_unique<APIPlaintextFrameHelper>(std::move(sock));
  helper->init();

  return {std::move(helper), read_fd};
}

// --- Write a single SensorStateResponse through plaintext framing ---
// Measures the full write path: header construction, varint encoding,
// iovec assembly, and socket write.

static void PlaintextFrame_WriteSensorState(benchmark::State &state) {
  auto [helper, read_fd] = create_plaintext_helper();
  uint8_t padding = helper->frame_header_padding();

  // Pre-init buffer to typical TCP MSS size to avoid benchmarking
  // heap allocation — in real use the buffer is reused across writes.
  APIBuffer buffer;
  buffer.reserve(1460);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      buffer.clear();
      SensorStateResponse msg;
      msg.key = 0x12345678;
      msg.state = 23.5f;
      msg.missing_state = false;

      uint32_t size = msg.calculate_size();
      buffer.resize(padding + size);
      ProtoWriteBuffer writer(&buffer, padding);
      msg.encode(writer);

      helper->write_protobuf_packet(SensorStateResponse::MESSAGE_TYPE, writer);

      if ((i & 0xFF) == 0)
        drain_socket(read_fd);
    }
    drain_socket(read_fd);
    benchmark::DoNotOptimize(helper.get());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);

  ::close(read_fd);
}
BENCHMARK(PlaintextFrame_WriteSensorState);

// --- Write a batch of 5 SensorStateResponses in one call ---
// Measures batched write: multiple messages assembled into one writev.

static void PlaintextFrame_WriteBatch5(benchmark::State &state) {
  auto [helper, read_fd] = create_plaintext_helper();
  uint8_t padding = helper->frame_header_padding();
  uint8_t footer = helper->frame_footer_size();

  // Pre-init buffer to typical TCP MSS size to avoid benchmarking
  // heap allocation — in real use the buffer is reused across writes.
  APIBuffer buffer;
  buffer.reserve(1460);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      buffer.clear();
      MessageInfo messages[5] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

      for (int j = 0; j < 5; j++) {
        uint16_t offset = buffer.size();
        SensorStateResponse msg;
        msg.key = static_cast<uint32_t>(j);
        msg.state = 23.5f + static_cast<float>(j);
        msg.missing_state = false;

        uint32_t size = msg.calculate_size();
        buffer.resize(offset + padding + size + footer);
        ProtoWriteBuffer writer(&buffer, offset + padding);
        msg.encode(writer);

        messages[j] = MessageInfo(SensorStateResponse::MESSAGE_TYPE, offset, size);
      }

      helper->write_protobuf_messages(ProtoWriteBuffer(&buffer, 0), std::span<const MessageInfo>(messages, 5));

      if ((i & 0xFF) == 0)
        drain_socket(read_fd);
    }
    drain_socket(read_fd);
    benchmark::DoNotOptimize(helper.get());
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);

  ::close(read_fd);
}
BENCHMARK(PlaintextFrame_WriteBatch5);

}  // namespace esphome::api::benchmarks

#endif  // USE_API_PLAINTEXT
