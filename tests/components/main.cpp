#include <gtest/gtest.h>

#include "esphome/components/logger/logger.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

/*
This special main.cpp replaces the default one.
It will run all the Google Tests found in all compiled cpp files and then exit with the result
See README.md for more information
*/

// Auto generated code by esphome
// ========== AUTO GENERATED INCLUDE BLOCK BEGIN ===========
// ========== AUTO GENERATED INCLUDE BLOCK END ==========="

void original_setup() {
  // This function won't be run.

  // ========== AUTO GENERATED CODE BEGIN ===========
  // =========== AUTO GENERATED CODE END ============
}

// The loop clock and current component one test moves must not leak into the next, like CORE.reset() in Python
class ResetAppListener : public ::testing::EmptyTestEventListener {
  void OnTestStart(const ::testing::TestInfo &) override {
    esphome::LoopBlockingGuard reset{nullptr, nullptr, 0};
    // Entities of the same kind share preference keys, so a value an earlier test saved would be restored
    if (esphome::global_preferences != nullptr)
      esphome::global_preferences->reset();
  }
};

void setup() {
  // Log functions call global_logger->log_vprintf_() without a null check,
  // so we must set up a Logger before any test that triggers logging.
  static esphome::logger::Logger test_logger(0);
  test_logger.set_log_level(ESPHOME_LOG_LEVEL);
  test_logger.pre_setup();

  ::testing::InitGoogleTest();
  ::testing::UnitTest::GetInstance()->listeners().Append(new ResetAppListener);
  int exit_code = RUN_ALL_TESTS();
  // A test folder that never reached the build would otherwise pass as an empty run
  if (::testing::UnitTest::GetInstance()->total_test_count() == 0) {
    fprintf(stderr, "No tests were linked into this binary\n");
    exit_code = 1;
  }
  exit(exit_code);
}

void loop() {}
