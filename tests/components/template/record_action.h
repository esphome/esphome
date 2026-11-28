#pragma once

#include "esphome/core/automation.h"

namespace esphome::template_::testing {

/// Keeps the last value a set trigger sent
template<typename T> class RecordAction : public Action<T> {
 public:
  void play(const T &value) override { this->seen = value; }
  T seen{};
};

}  // namespace esphome::template_::testing
