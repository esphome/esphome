#include <benchmark/benchmark.h>

#include "esphome/components/logger/logger.h"

/*
This special main.cpp provides the entry point for Google Benchmark.
It replaces the default ESPHome main with a benchmark runner.

*/

// Auto generated code by esphome
// ========== AUTO GENERATED INCLUDE BLOCK BEGIN ===========
// ========== AUTO GENERATED INCLUDE BLOCK END ===========

void original_setup() {
  // Code-generated App initialization (pre_setup, area/device registration, etc.)

  // ========== AUTO GENERATED CODE BEGIN ===========
  // =========== AUTO GENERATED CODE END ============
}

void setup() {
  // Run auto-generated initialization (App.pre_setup, area/device registration,
  // looping_components_.init, etc.) so benchmarks that use App work correctly.
  original_setup();

  // Log functions call global_logger->log_vprintf_() without a null check,
  // so we must set up a Logger before any test that triggers logging.
  static esphome::logger::Logger test_logger(0);
  test_logger.set_log_level(ESPHOME_LOG_LEVEL);
  test_logger.pre_setup();

  int argc = 1;
  char arg0[] = "benchmark";
  char *argv[] = {arg0, nullptr};
  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  exit(0);
}

void loop() {}
