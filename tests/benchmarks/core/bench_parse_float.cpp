#include <benchmark/benchmark.h>
#include <cstdlib>

#include "esphome/core/helpers.h"

namespace esphome::benchmarks {

// Inner iteration count to amortize CodSpeed instrumentation overhead.
static constexpr int kInnerIterations = 2000;

// Short plain values take parse_float()'s fast path; the others take the full parser (libc's strtof before).
static const char *const PLAIN[] = {"0.5", "255", "-12.75", "21.3"};
static const char *const EXPONENT[] = {"1e3", "2.5e-2", "-6.02E5", "1.5e+1"};
static const char *const PRECISE[] = {"3.14159265358979", "0.000123456789", "123456789.125", "-0.1000000000000000055"};

template<size_t N> static void run_parse_number(benchmark::State &state, const char *const (&inputs)[N]) {
  for (auto _ : state) {
    float sum = 0.0f;
    for (int i = 0; i < kInnerIterations; i++)
      sum += parse_number<float>(inputs[i % N]).value_or(0.0f);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}

template<size_t N> static void run_parse_float_prefix(benchmark::State &state, const char *const (&inputs)[N]) {
  for (auto _ : state) {
    float sum = 0.0f;
    for (int i = 0; i < kInnerIterations; i++)
      sum += parse_float_prefix(inputs[i % N], nullptr);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}

// The libc parser parse_float() used before, for comparison
template<size_t N> static void run_strtof(benchmark::State &state, const char *const (&inputs)[N]) {
  for (auto _ : state) {
    float sum = 0.0f;
    for (int i = 0; i < kInnerIterations; i++)
      sum += ::strtof(inputs[i % N], nullptr);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * kInnerIterations);
}

static void ParseNumberFloat_Plain(benchmark::State &state) { run_parse_number(state, PLAIN); }
BENCHMARK(ParseNumberFloat_Plain);

static void ParseNumberFloat_Exponent(benchmark::State &state) { run_parse_number(state, EXPONENT); }
BENCHMARK(ParseNumberFloat_Exponent);

static void ParseNumberFloat_Precise(benchmark::State &state) { run_parse_number(state, PRECISE); }
BENCHMARK(ParseNumberFloat_Precise);

static void ParseFloatPrefix_Plain(benchmark::State &state) { run_parse_float_prefix(state, PLAIN); }
BENCHMARK(ParseFloatPrefix_Plain);

static void ParseFloatPrefix_Exponent(benchmark::State &state) { run_parse_float_prefix(state, EXPONENT); }
BENCHMARK(ParseFloatPrefix_Exponent);

static void ParseFloatPrefix_Precise(benchmark::State &state) { run_parse_float_prefix(state, PRECISE); }
BENCHMARK(ParseFloatPrefix_Precise);

static void Strtof_Plain(benchmark::State &state) { run_strtof(state, PLAIN); }
BENCHMARK(Strtof_Plain);

static void Strtof_Exponent(benchmark::State &state) { run_strtof(state, EXPONENT); }
BENCHMARK(Strtof_Exponent);

static void Strtof_Precise(benchmark::State &state) { run_strtof(state, PRECISE); }
BENCHMARK(Strtof_Precise);

}  // namespace esphome::benchmarks
