#pragma once

#include "esphome/core/component.h"
#include <vector>
#include <string>

namespace esphome {
namespace scheduler_string_lifetime_component {

class SchedulerStringLifetimeComponent : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void run_string_lifetime_test();

  // Individual test methods exposed as services
  void run_test1();
  void run_test2();
  void run_test3();
  void run_test4();
  void run_test5();
  void run_final_check();

 private:
  void test_temporary_string_lifetime();
  void test_scope_exit_string();
  void test_vector_reallocation();
  void test_string_move_semantics();
  void test_lambda_capture_lifetime();

  int tests_passed_{0};
  int tests_failed_{0};
};

}  // namespace scheduler_string_lifetime_component
}  // namespace esphome
