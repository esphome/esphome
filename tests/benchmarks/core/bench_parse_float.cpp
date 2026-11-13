#include <benchmark/benchmark.h>

#include "esphome/core/helpers.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

static const char *const PLAIN[] = {"0.5", "255", "-12.75", "21.3"};
static const char *const EXPONENT[] = {"1e3", "2.5e-2", "-6.02E5", "1.5e+1"};
static const char *const PRECISE[] = {"3.14159265358979", "0.000123456789", "123456789.125", "-0.1000000000000000055"};
// Home Assistant states that are not numbers
static const char *const REJECT[] = {"unavailable", "unknown", "on", "off"};

template<size_t N, typename F> static void run(benchmark::State &state, const char *const (&inputs)[N], F parse) {
  for (auto _ : state) {
    float sum = 0.0f;
    for (int i = 0; i < kInnerIterations; i++)
      sum += parse(inputs[i % N]);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}

static float parse_number_float(const char *str) { return parse_number<float>(str).value_or(0.0f); }
static float parse_prefix(const char *str) { return parse_float_prefix(str, nullptr); }

static void ParseNumberFloat_Plain(benchmark::State &state) { run(state, PLAIN, parse_number_float); }
BENCHMARK(ParseNumberFloat_Plain);

static void ParseNumberFloat_Exponent(benchmark::State &state) { run(state, EXPONENT, parse_number_float); }
BENCHMARK(ParseNumberFloat_Exponent);

static void ParseNumberFloat_Precise(benchmark::State &state) { run(state, PRECISE, parse_number_float); }
BENCHMARK(ParseNumberFloat_Precise);

static void ParseNumberFloat_Reject(benchmark::State &state) { run(state, REJECT, parse_number_float); }
BENCHMARK(ParseNumberFloat_Reject);

static void ParseFloatPrefix_Exponent(benchmark::State &state) { run(state, EXPONENT, parse_prefix); }
BENCHMARK(ParseFloatPrefix_Exponent);

static void ParseFloatPrefix_Precise(benchmark::State &state) { run(state, PRECISE, parse_prefix); }
BENCHMARK(ParseFloatPrefix_Precise);

}  // namespace esphome::benchmarks
