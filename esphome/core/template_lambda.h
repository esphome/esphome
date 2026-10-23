#pragma once

#include "esphome/core/optional.h"

namespace esphome {

/** Lightweight wrapper for template platform lambdas (stateless function pointers only).
 *
 * This optimizes template platforms by storing only a function pointer (4 bytes on ESP32)
 * instead of std::function (16-32 bytes).
 *
 * IMPORTANT: This only supports stateless lambdas (no captures). The set_template() method
 * is an internal API used by YAML codegen, not intended for external use.
 *
 * Lambdas must return optional<T> to support the pattern:
 *   return {};    // Don't publish a value
 *   return 42.0;  // Publish this value
 *
 * operator() returns optional<T>, returning nullopt when no lambda is set (nullptr check).
 *
 * @tparam T The return type (e.g., float for sensor values)
 * @tparam Args Optional arguments for the lambda
 */
template<typename T, typename... Args> class TemplateLambda {
 public:
  TemplateLambda() : f_(nullptr) {}

  /** Set the lambda function pointer.
   * INTERNAL API: Only for use by YAML codegen.
   * Only stateless lambdas (no captures) are supported.
   */
  void set(optional<T> (*f)(Args...)) { this->f_ = f; }

  /** Check if a lambda is set */
  bool has_value() const { return this->f_ != nullptr; }

  /** Call the lambda, returning nullopt if no lambda is set */
  optional<T> operator()(Args &&...args) {
    if (this->f_ == nullptr)
      return nullopt;
    return this->f_(std::forward<Args>(args)...);
  }

  /** Alias for operator() for compatibility */
  optional<T> call(Args &&...args) { return (*this)(std::forward<Args>(args)...); }

 protected:
  optional<T> (*f_)(Args...);  // Function pointer (4 bytes on ESP32)
};

}  // namespace esphome
