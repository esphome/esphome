#include "esphome/core/defines.h"
#ifdef USE_API_PLAINTEXT

#include <benchmark/benchmark.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/api/api_frame_helper_plaintext.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_buffer.h"

namespace esphome::api::benchmarks {

static constexpr int kInnerIterations = 2000;

// Helper to drain accumulated data from the read side of a socketpair
// to prevent the write side from blocking.
static void drain_socket(int fd) {
  char buf[65536];
  while (::read(fd, buf, sizeof(buf)) > 0) {
  }
}

// Helper to create a non-blocking socketpair with an APIPlaintextFrameHelper
// on the write end. Returns the helper and the read-side fd.
static std::pair<std::unique_ptr<APIPlaintextFrameHelper>, int> create_plaintext_helper() {
  int fds[2];
  ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

  // Make both ends non-blocking
  int flags0 = ::fcntl(fds[0], F_GETFL, 0);
  ::fcntl(fds[0], F_SETFL, flags0 | O_NONBLOCK);
  int flags1 = ::fcntl(fds[1], F_GETFL, 0);
  ::fcntl(fds[1], F_SETFL, flags1 | O_NONBLOCK);

  // Increase socket buffer sizes to reduce drain frequency
  int bufsize = 1024 * 1024;
  ::setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
  ::setsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));

  auto sock = std::make_unique<socket::Socket>(fds[0]);
  auto helper = std::make_unique<APIPlaintextFrameHelper>(std::move(sock));
  helper->init();

  return {std::move(helper), fds[1]};
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

      helper->write_protobuf_packet(38, writer);

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

        messages[j] = MessageInfo(38, offset, size);
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
