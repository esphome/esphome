#include <benchmark/benchmark.h>

#include "esphome/components/logger/logger.h"

/*
This special main.cpp provides the entry point for Google Benchmark.
It replaces the default ESPHome main with a benchmark runner.

*/

// Auto generated code by esphome
// ========== AUTO GENERATED INCLUDE BLOCK BEGIN ===========
// ========== AUTO GENERATED INCLUDE BLOCK END ==========="

void original_setup() {
  // This function won't be run.

  // ========== AUTO GENERATED CODE BEGIN ===========
  // =========== AUTO GENERATED CODE END ============
}

void setup() {
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
