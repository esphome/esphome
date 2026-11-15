#include <benchmark/benchmark.h>

#include "esphome/components/remote_base/nec_protocol.h"
#include "esphome/components/remote_base/rc_switch_protocol.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::benchmarks {

// Every in-tree dumper, built by the codegen in __init__.py as `dump: all` does
extern remote_base::RemoteReceiverDumperBase *const DUMP_ALL[];
extern const size_t DUMP_ALL_COUNT;

using remote_base::RawTimings;
using remote_base::RemoteReceiveData;

// Amortizes CodSpeed's per-iteration instrumentation cost
static constexpr int kInnerIterations = 2000;
static constexpr uint32_t TOLERANCE = 25;
static constexpr auto TOLERANCE_MODE = remote_base::TOLERANCE_MODE_PERCENTAGE;
static constexpr uint16_t NEC_ADDRESS = 0x7F80;
static constexpr uint16_t NEC_COMMAND = 0xF20D;

static RawTimings nec_frame() {
  remote_base::RemoteTransmitData dst;
  remote_base::NECProtocol().encode(&dst, {.address = NEC_ADDRESS, .command = NEC_COMMAND, .command_repeats = 1});
  return dst.get_data();
}

// Valid header and 31 bits, then a last bit space that is neither a one nor a zero
static RawTimings nec_near_miss_frame() {
  RawTimings raw = nec_frame();
  raw[raw.size() - 2] = -1100;
  return raw;
}

static RawTimings rc_switch_frame() {
  remote_base::RemoteTransmitData dst;
  remote_base::rc_switch_protocol(1).transmit(&dst, 0x5A5A5A, 24);
  return dst.get_data();
}

// 433 MHz style noise: 64 alternating marks and spaces of 100-1500 us from a fixed LCG
static RawTimings noise_frame() {
  RawTimings raw;
  uint32_t seed = 12345;
  for (int i = 0; i < 64; i++) {
    seed = seed * 1103515245 + 12345;
    int32_t len = 100 + static_cast<int32_t>((seed >> 16) % 1400);
    raw.push_back(i % 2 == 0 ? len : -len);
  }
  return raw;
}

template<typename P> static void decode_bench(benchmark::State &state, const RawTimings &raw, bool expect) {
  P proto;
  RemoteReceiveData check(raw, TOLERANCE, TOLERANCE_MODE);
  if (proto.decode(check).has_value() != expect) {
    state.SkipWithError("unexpected decode result");
    return;
  }
  for (auto _ : state) {
    for (int i = 0; i < kInnerIterations; i++) {
      RemoteReceiveData src(raw, TOLERANCE, TOLERANCE_MODE);
      auto res = proto.decode(src);
      benchmark::DoNotOptimize(res);
    }
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}

// --- Single decoder ---

static void RemoteDecode_NEC_Match(benchmark::State &state) {
  decode_bench<remote_base::NECProtocol>(state, nec_frame(), true);
}
BENCHMARK(RemoteDecode_NEC_Match);

// Rejected at the header, the common case
static void RemoteDecode_NEC_Noise(benchmark::State &state) {
  decode_bench<remote_base::NECProtocol>(state, noise_frame(), false);
}
BENCHMARK(RemoteDecode_NEC_Noise);

// Rejected at the last bit, the most work a failed decode does
static void RemoteDecode_NEC_NearMiss(benchmark::State &state) {
  decode_bench<remote_base::NECProtocol>(state, nec_near_miss_frame(), false);
}
BENCHMARK(RemoteDecode_NEC_NearMiss);

static void RemoteDecode_RCSwitch_Match(benchmark::State &state) {
  decode_bench<remote_base::RCSwitchBase>(state, rc_switch_frame(), true);
}
BENCHMARK(RemoteDecode_RCSwitch_Match);

// Tries all eight RCSwitch timings before giving up
static void RemoteDecode_RCSwitch_Noise(benchmark::State &state) {
  decode_bench<remote_base::RCSwitchBase>(state, noise_frame(), false);
}
BENCHMARK(RemoteDecode_RCSwitch_Noise);

// --- Receiver fan-out ---

class BenchReceiver : public remote_base::RemoteReceiverBase {
 public:
  BenchReceiver() : RemoteReceiverBase(nullptr) {}
  void run(benchmark::State &state, const RawTimings &raw) {
    const remote_base::RawTimingsView view(raw);
    for (auto _ : state) {
      for (int i = 0; i < kInnerIterations; i++)
        this->call_listeners_dumpers_(view);
    }
    state.SetItemsProcessed(state.iterations() * kInnerIterations);
  }
};

// Log formatting is measured: DEBUG level, and the Logger at baud 0 formats without writing to a console
static void dump_all_bench(benchmark::State &state, const RawTimings &raw) {
  BenchReceiver receiver;
  receiver.set_dumpers(DUMP_ALL, DUMP_ALL_COUNT);
  receiver.run(state, raw);
}

// Every dumper tries the frame; the matching ones log it
static void RemoteDumpAll_NEC(benchmark::State &state) { dump_all_bench(state, nec_frame()); }
BENCHMARK(RemoteDumpAll_NEC);

// Pronto always decodes, so the raw fallback never runs and Pronto's string building and logging dominate
static void RemoteDumpAll_Noise(benchmark::State &state) { dump_all_bench(state, noise_frame()); }
BENCHMARK(RemoteDumpAll_Noise);

// Every sensor shares the NEC address; sensor 0 has the frame's command when match is true
static void listeners_bench(benchmark::State &state, const RawTimings &raw, bool match) {
  BenchReceiver receiver;
  remote_base::NECBinarySensor sensors[REMOTE_BASE_LISTENER_COUNT];
  for (uint16_t i = 0; i < REMOTE_BASE_LISTENER_COUNT; i++) {
    const uint16_t command = match && i == 0 ? NEC_COMMAND : i;
    sensors[i].set_data({.address = NEC_ADDRESS, .command = command, .command_repeats = 1});
    receiver.register_listener(&sensors[i]);
  }
  receiver.run(state, raw);
}

// Eight NEC binary sensors, none matching the noise
static void RemoteListeners_NEC8_Noise(benchmark::State &state) { listeners_bench(state, noise_frame(), false); }
BENCHMARK(RemoteListeners_NEC8_Noise);

// Every sensor decodes the full frame; one matches and pulses its state on and off
static void RemoteListeners_NEC8_Match(benchmark::State &state) { listeners_bench(state, nec_frame(), true); }
BENCHMARK(RemoteListeners_NEC8_Match);

}  // namespace esphome::benchmarks
