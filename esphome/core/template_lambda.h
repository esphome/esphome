#pragma once

#include <functional>
#include <concepts>
#include "esphome/core/optional.h"

namespace esphome {

/** Helper class for template platforms that stores either a stateless lambda (function pointer)
 * or a stateful lambda (std::function pointer).
 *
 * This provides backward compatibility with PR #11555 while maintaining the optimization:
 * - Stateless lambdas (no capture) → function pointer (4 bytes on ESP32)
 * - Stateful lambdas (with capture) → pointer to std::function (4 bytes on ESP32)
 * Total size: enum (1 byte) + union (4 bytes) + padding = 8 bytes (same as PR #11555)
 *
 * Both lambda types must return optional<T> (as YAML codegen does) to support the pattern:
 *   return {};  // Don't publish a value
 *   return 42.0;  // Publish this value
 *
 * operator() returns optional<T>, returning nullopt when no lambda is set (type == NONE).
 * This eliminates redundant "is lambda set" checks by reusing optional's discriminator.
 *
 * @tparam T The return type (e.g., float for TemplateLambda<optional<float>>)
 * @tparam Args Optional arguments for the lambda
 */
template<typename T, typename... Args> class TemplateLambda {
 public:
  TemplateLambda() : type_(NONE) {}

  // For stateless lambdas: use function pointer
  template<typename F>
  requires std::invocable<F, Args...> && std::convertible_to < F, optional<T>(*)
  (Args...) > void set(F f) {
    this->reset();
    this->type_ = STATELESS_LAMBDA;
    this->stateless_f_ = f;  // Implicit conversion to function pointer
  }

  // For stateful lambdas: use std::function pointer
  template<typename F>
  requires std::invocable<F, Args...> &&
      (!std::convertible_to<F, optional<T> (*)(Args...)>) &&std::convertible_to<std::invoke_result_t<F, Args...>,
                                                                                optional<T>> void set(F &&f) {
    this->reset();
    this->type_ = LAMBDA;
    this->f_ = new std::function<optional<T>(Args...)>(std::forward<F>(f));
  }

  ~TemplateLambda() { this->reset(); }

  // Copy constructor
  TemplateLambda(const TemplateLambda &) = delete;
  TemplateLambda &operator=(const TemplateLambda &) = delete;

  // Move constructor
  TemplateLambda(TemplateLambda &&other) noexcept : type_(other.type_) {
    if (type_ == LAMBDA) {
      this->f_ = other.f_;
      other.f_ = nullptr;
    } else if (type_ == STATELESS_LAMBDA) {
      this->stateless_f_ = other.stateless_f_;
    }
    other.type_ = NONE;
  }

  TemplateLambda &operator=(TemplateLambda &&other) noexcept {
    if (this != &other) {
      this->reset();
      this->type_ = other.type_;
      if (type_ == LAMBDA) {
        this->f_ = other.f_;
        other.f_ = nullptr;
      } else if (type_ == STATELESS_LAMBDA) {
        this->stateless_f_ = other.stateless_f_;
      }
      other.type_ = NONE;
    }
    return *this;
  }

  bool has_value() const { return this->type_ != NONE; }

  // Returns optional<T>, returning nullopt if no lambda is set
  optional<T> operator()(Args... args) {
    switch (this->type_) {
      case STATELESS_LAMBDA:
        return this->stateless_f_(args...);  // Direct function pointer call
      case LAMBDA:
        return (*this->f_)(args...);  // std::function call via pointer
      case NONE:
      default:
        return nullopt;  // No lambda set
    }
  }

  optional<T> call(Args... args) { return (*this)(args...); }

 protected:
  void reset() {
    if (this->type_ == LAMBDA) {
      delete this->f_;
      this->f_ = nullptr;
    }
    this->type_ = NONE;
  }

  enum : uint8_t {
    NONE,
    STATELESS_LAMBDA,
    LAMBDA,
  } type_;

  union {
    optional<T> (*stateless_f_)(Args...);     // Function pointer (4 bytes on ESP32)
    std::function<optional<T>(Args...)> *f_;  // Pointer to std::function (4 bytes on ESP32)
  };
};

}  // namespace esphome
