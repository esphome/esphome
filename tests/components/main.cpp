#include <gtest/gtest.h>
#include <vector>
#include "esphome/components/logger/logger.h"
#include "esphome/core/application.h"

/*
This special main.cpp replaces the default one.
It will run all the Google Tests found in all compiled cpp files and then exit with the result
See README.md for more information
*/

// Auto generated code by esphome
// ========== AUTO GENERATED INCLUDE BLOCK BEGIN ===========
// ========== AUTO GENERATED INCLUDE BLOCK END ===========

// Components registered during individual unit tests.
static std::vector<esphome::Component *> &get_test_components() {
  static std::vector<esphome::Component *> test_components;
  return test_components;
}

void register_component(esphome::Component *comp) { get_test_components().push_back(comp); }

void original_setup() {
  // This function won't be run by the test runner.
  // It's here for the codegen markers.
  // ========== AUTO GENERATED CODE BEGIN ===========
  // =========== AUTO GENERATED CODE END ============
}

void setup() {
  // Construct App via placement new — see application.cpp for storage details.
  new (&esphome::App) esphome::Application();

  // Log functions call global_logger->log_vprintf_() without a null check,
  static esphome::logger::Logger test_logger(0);
  test_logger.set_log_level(ESPHOME_LOG_LEVEL);
  test_logger.pre_setup();

  ::testing::InitGoogleTest();
  int exit_code = RUN_ALL_TESTS();

  // Register all components gathered during tests with App,
  // so App.~Application() will delete them.
  // We do this here because setup() is a friend of Application.
  for (auto *comp : get_test_components()) {
    esphome::App.register_component_(comp);
  }
  get_test_components().clear();

  // On host/tests, App is a placement-newed object.
  // We must call its destructor explicitly to trigger cleanup (deleting components).
  esphome::App.~Application();

  exit(exit_code);
}

void loop() {}
