#include <benchmark/benchmark.h>

#include "esphome/components/cover/cover.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

// Minimal Cover for benchmarking — control() is a no-op.
class BenchCover : public cover::Cover {
 public:
  cover::CoverTraits get_traits() override { return this->traits_; }
  void configure(const char *name) { this->configure_entity_(name, 0x12345678, 0); }

  cover::CoverTraits traits_;

 protected:
  void control(const cover::CoverCall & /*call*/) override {}
};

// --- Cover::publish_state() with position updates ---
// Measures the publish path for a garage door reporting position
// during open/close — the hot path during movement.

static void CoverPublish_Position(benchmark::State &state) {
  BenchCover cover;
  cover.configure("test_cover");
  cover.traits_.set_supports_position(true);
  cover.traits_.set_supports_tilt(false);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      cover.position = static_cast<float>(i % 101) / 100.0f;
      cover.current_operation = (i % 2 == 0) ? cover::COVER_OPERATION_OPENING : cover::COVER_OPERATION_CLOSING;
      cover.publish_state(false);
    }
    benchmark::DoNotOptimize(cover.position);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CoverPublish_Position);

// --- Cover::publish_state() with callback ---
// Measures callback dispatch overhead.

static void CoverPublish_WithCallback(benchmark::State &state) {
  BenchCover cover;
  cover.configure("test_cover");
  cover.traits_.set_supports_position(true);

  uint64_t callback_count = 0;
  cover.add_on_state_callback([&callback_count]() { callback_count++; });

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      cover.position = static_cast<float>(i % 101) / 100.0f;
      cover.publish_state(false);
    }
    benchmark::DoNotOptimize(callback_count);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CoverPublish_WithCallback);

// --- CoverCall::perform() open/close cycle ---
// Measures the full call path: validation + control delegation.

static void CoverCall_OpenClose(benchmark::State &state) {
  BenchCover cover;
  cover.configure("test_cover");
  cover.traits_.set_supports_position(true);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      if (i % 2 == 0) {
        cover.make_call().set_command_open().perform();
      } else {
        cover.make_call().set_command_close().perform();
      }
    }
    benchmark::DoNotOptimize(cover.position);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CoverCall_OpenClose);

// --- CoverCall::perform() set position ---
// Measures the position-setting call path.

static void CoverCall_SetPosition(benchmark::State &state) {
  BenchCover cover;
  cover.configure("test_cover");
  cover.traits_.set_supports_position(true);

  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      float pos = static_cast<float>(i % 101) / 100.0f;
      cover.make_call().set_position(pos).perform();
    }
    benchmark::DoNotOptimize(cover.position);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}
BENCHMARK(CoverCall_SetPosition);

}  // namespace esphome::benchmarks
