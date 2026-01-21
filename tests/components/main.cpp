#include <gtest/gtest.h>

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

void setup() {
  ::testing::InitGoogleTest();
  int exit_code = RUN_ALL_TESTS();
  exit(exit_code);
}

void loop() {}
