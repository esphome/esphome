#include "esphome/core/defines.h"
#ifdef USE_API_PLAINTEXT

#include <benchmark/benchmark.h>
#include <unistd.h>

#include "bench_helpers.h"
#include "esphome/components/api/api_frame_helper_plaintext.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_buffer.h"

namespace esphome::api::benchmarks {

static constexpr int kInnerIterations = 2000;

// Helper to create a TCP loopback connection with an APIPlaintextFrameHelper
// on the write end. Returns the helper and the read-side fd.
static std::pair<std::unique_ptr<APIPlaintextFrameHelper>, int> create_plaintext_helper() {
  auto [sock, read_fd] = create_tcp_loopback();
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
