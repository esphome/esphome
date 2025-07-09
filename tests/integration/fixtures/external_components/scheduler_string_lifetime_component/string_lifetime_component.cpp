#include "string_lifetime_component.h"
#include "esphome/core/log.h"
#include <memory>
#include <thread>
#include <chrono>

namespace esphome {
namespace scheduler_string_lifetime_component {

static const char *const TAG = "scheduler_string_lifetime";

void SchedulerStringLifetimeComponent::setup() { ESP_LOGCONFIG(TAG, "SchedulerStringLifetimeComponent setup"); }

void SchedulerStringLifetimeComponent::run_string_lifetime_test() {
  ESP_LOGI(TAG, "Starting string lifetime tests");

  this->tests_passed_ = 0;
  this->tests_failed_ = 0;

  // Run each test
  test_temporary_string_lifetime();
  test_scope_exit_string();
  test_vector_reallocation();
  test_string_move_semantics();
  test_lambda_capture_lifetime();
}

void SchedulerStringLifetimeComponent::run_test1() {
  test_temporary_string_lifetime();
  // Wait for all callbacks to execute
  this->set_timeout("test1_complete", 10, []() { ESP_LOGI(TAG, "Test 1 complete"); });
}

void SchedulerStringLifetimeComponent::run_test2() {
  test_scope_exit_string();
  // Wait for all callbacks to execute
  this->set_timeout("test2_complete", 20, []() { ESP_LOGI(TAG, "Test 2 complete"); });
}

void SchedulerStringLifetimeComponent::run_test3() {
  test_vector_reallocation();
  // Wait for all callbacks to execute
  this->set_timeout("test3_complete", 60, []() { ESP_LOGI(TAG, "Test 3 complete"); });
}

void SchedulerStringLifetimeComponent::run_test4() {
  test_string_move_semantics();
  // Wait for all callbacks to execute
  this->set_timeout("test4_complete", 35, []() { ESP_LOGI(TAG, "Test 4 complete"); });
}

void SchedulerStringLifetimeComponent::run_test5() {
  test_lambda_capture_lifetime();
  // Wait for all callbacks to execute
  this->set_timeout("test5_complete", 50, []() { ESP_LOGI(TAG, "Test 5 complete"); });
}

void SchedulerStringLifetimeComponent::run_final_check() {
  ESP_LOGI(TAG, "Tests passed: %d", this->tests_passed_);
  ESP_LOGI(TAG, "Tests failed: %d", this->tests_failed_);

  if (this->tests_failed_ == 0) {
    ESP_LOGI(TAG, "SUCCESS: All string lifetime tests passed!");
  } else {
    ESP_LOGE(TAG, "FAILURE: %d string lifetime tests failed!", this->tests_failed_);
  }
  ESP_LOGI(TAG, "String lifetime tests complete");
}

void SchedulerStringLifetimeComponent::test_temporary_string_lifetime() {
  ESP_LOGI(TAG, "Test 1: Temporary string lifetime for timeout names");

  // Test with a temporary string that goes out of scope immediately
  {
    std::string temp_name = "temp_callback_" + std::to_string(12345);

    // Schedule with temporary string name - scheduler must copy/store this
    this->set_timeout(temp_name, 1, [this]() {
      ESP_LOGD(TAG, "Callback for temp string name executed");
      this->tests_passed_++;
    });

    // String goes out of scope here, but scheduler should have made a copy
  }

  // Test with rvalue string as name
  this->set_timeout(std::string("rvalue_test"), 2, [this]() {
    ESP_LOGD(TAG, "Rvalue string name callback executed");
    this->tests_passed_++;
  });

  // Test cancelling with reconstructed string
  {
    std::string cancel_name = "cancel_test_" + std::to_string(999);
    this->set_timeout(cancel_name, 100, [this]() {
      ESP_LOGE(TAG, "This should have been cancelled!");
      this->tests_failed_++;
    });
  }  // cancel_name goes out of scope

  // Reconstruct the same string to cancel
  std::string cancel_name_2 = "cancel_test_" + std::to_string(999);
  bool cancelled = this->cancel_timeout(cancel_name_2);
  if (cancelled) {
    ESP_LOGD(TAG, "Successfully cancelled with reconstructed string");
    this->tests_passed_++;
  } else {
    ESP_LOGE(TAG, "Failed to cancel with reconstructed string");
    this->tests_failed_++;
  }
}

void SchedulerStringLifetimeComponent::test_scope_exit_string() {
  ESP_LOGI(TAG, "Test 2: Scope exit string names");

  // Create string names in a limited scope
  {
    std::string scoped_name = "scoped_timeout_" + std::to_string(555);

    // Schedule with scoped string name
    this->set_timeout(scoped_name, 3, [this]() {
      ESP_LOGD(TAG, "Scoped name callback executed");
      this->tests_passed_++;
    });

    // scoped_name goes out of scope here
  }

  // Test with dynamically allocated string name
  {
    auto *dynamic_name = new std::string("dynamic_timeout_" + std::to_string(777));

    this->set_timeout(*dynamic_name, 4, [this, dynamic_name]() {
      ESP_LOGD(TAG, "Dynamic string name callback executed");
      this->tests_passed_++;
      delete dynamic_name;  // Clean up in callback
    });

    // Pointer goes out of scope but string object remains until callback
  }

  // Test multiple timeouts with same dynamically created name
  for (int i = 0; i < 3; i++) {
    std::string loop_name = "loop_timeout_" + std::to_string(i);
    this->set_timeout(loop_name, 5 + i * 1, [this, i]() {
      ESP_LOGD(TAG, "Loop timeout %d executed", i);
      this->tests_passed_++;
    });
    // loop_name destroyed and recreated each iteration
  }
}

void SchedulerStringLifetimeComponent::test_vector_reallocation() {
  ESP_LOGI(TAG, "Test 3: Vector reallocation stress on timeout names");

  // Create a vector that will reallocate
  std::vector<std::string> names;
  names.reserve(2);  // Small initial capacity to force reallocation

  // Schedule callbacks with string names from vector
  for (int i = 0; i < 10; i++) {
    names.push_back("vector_cb_" + std::to_string(i));
    // Use the string from vector as timeout name
    this->set_timeout(names.back(), 8 + i * 1, [this, i]() {
      ESP_LOGV(TAG, "Vector name callback %d executed", i);
      this->tests_passed_++;
    });
  }

  // Force reallocation by adding more elements
  // This will move all strings to new memory locations
  for (int i = 10; i < 50; i++) {
    names.push_back("realloc_trigger_" + std::to_string(i));
  }

  // Add more timeouts after reallocation to ensure old names still work
  for (int i = 50; i < 55; i++) {
    names.push_back("post_realloc_" + std::to_string(i));
    this->set_timeout(names.back(), 20 + (i - 50), [this]() {
      ESP_LOGV(TAG, "Post-reallocation callback executed");
      this->tests_passed_++;
    });
  }

  // Clear the vector while timeouts are still pending
  names.clear();
  ESP_LOGD(TAG, "Vector cleared - all string names destroyed");
}

void SchedulerStringLifetimeComponent::test_string_move_semantics() {
  ESP_LOGI(TAG, "Test 4: String move semantics for timeout names");

  // Test moving string names
  std::string original = "move_test_original";
  std::string moved = std::move(original);

  // Schedule with moved string as name
  this->set_timeout(moved, 30, [this]() {
    ESP_LOGD(TAG, "Moved string name callback executed");
    this->tests_passed_++;
  });

  // original is now empty, try to use it as a different timeout name
  original = "reused_after_move";
  this->set_timeout(original, 32, [this]() {
    ESP_LOGD(TAG, "Reused string name callback executed");
    this->tests_passed_++;
  });
}

void SchedulerStringLifetimeComponent::test_lambda_capture_lifetime() {
  ESP_LOGI(TAG, "Test 5: Complex timeout name scenarios");

  // Test scheduling with name built in lambda
  [this]() {
    std::string lambda_name = "lambda_built_name_" + std::to_string(888);
    this->set_timeout(lambda_name, 38, [this]() {
      ESP_LOGD(TAG, "Lambda-built name callback executed");
      this->tests_passed_++;
    });
  }();  // Lambda executes and lambda_name is destroyed

  // Test with shared_ptr name
  auto shared_name = std::make_shared<std::string>("shared_ptr_timeout");
  this->set_timeout(*shared_name, 40, [this, shared_name]() {
    ESP_LOGD(TAG, "Shared_ptr name callback executed");
    this->tests_passed_++;
  });
  shared_name.reset();  // Release the shared_ptr

  // Test overwriting timeout with same name
  std::string overwrite_name = "overwrite_test";
  this->set_timeout(overwrite_name, 1000, [this]() {
    ESP_LOGE(TAG, "This should have been overwritten!");
    this->tests_failed_++;
  });

  // Overwrite with shorter timeout
  this->set_timeout(overwrite_name, 42, [this]() {
    ESP_LOGD(TAG, "Overwritten timeout executed");
    this->tests_passed_++;
  });

  // Test very long string name
  std::string long_name;
  for (int i = 0; i < 100; i++) {
    long_name += "very_long_timeout_name_segment_" + std::to_string(i) + "_";
  }
  this->set_timeout(long_name, 44, [this]() {
    ESP_LOGD(TAG, "Very long name timeout executed");
    this->tests_passed_++;
  });

  // Test empty string as name
  this->set_timeout("", 46, [this]() {
    ESP_LOGD(TAG, "Empty string name timeout executed");
    this->tests_passed_++;
  });
}

}  // namespace scheduler_string_lifetime_component
}  // namespace esphome
