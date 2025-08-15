#pragma once

namespace esphome {

using std::enable_if_t;
using std::is_invocable;

template<typename T> class Templatable {
 public:
  Templatable() : type_(NONE) {}

  template<typename F, enable_if_t<!is_invocable<F>::value, int> = 0> Templatable(F value) : type_(VALUE) {
    new (&this->value_) T(std::move(value));
  }

  template<typename F, enable_if_t<is_invocable<F>::value, int> = 0> Templatable(F f) : type_(LAMBDA) {
    this->function_ = new std::function<T()>(std::move(f));
  }

  // Copy constructor
  Templatable(const Templatable &other) : type_(other.type_) {
    if (type_ == VALUE) {
      new (&this->value_) T(other.value_);
    } else if (type_ == LAMBDA) {
      this->function_ = new std::function<T()>(*other.function_);
    }
  }

  // Move constructor
  Templatable(Templatable &&other) noexcept : type_(other.type_) {
    if (type_ == VALUE) {
      new (&this->value_) T(std::move(other.value_));
    } else if (type_ == LAMBDA) {
      this->function_ = other.function_;
      other.function_ = nullptr;
    }
    other.type_ = NONE;
  }

  // Assignment operators
  Templatable &operator=(const Templatable &other) {
    if (this != &other) {
      this->~Templatable();
      new (this) Templatable(other);
    }
    return *this;
  }

  Templatable &operator=(Templatable &&other) noexcept {
    if (this != &other) {
      this->~Templatable();
      new (this) Templatable(std::move(other));
    }
    return *this;
  }

  ~Templatable() {
    if (type_ == VALUE) {
      this->value_.~T();
    } else if (type_ == LAMBDA) {
      delete this->function_;
    }
  }

  bool has_value() { return this->type_ != NONE; }

  T value() {
    if (this->type_ == LAMBDA) {
      return (*this->function_)();
    }
    // return value also when none
    return this->type_ == VALUE ? this->value_ : T{};
  }

  optional<T> optional_value() {
    if (!this->has_value()) {
      return {};
    }
    return this->value();
  }

  T value_or(T default_value) {
    if (!this->has_value()) {
      return default_value;
    }
    return this->value();
  }

 protected:
  enum : uint8_t {
    NONE,
    VALUE,
    LAMBDA,
  } type_;

  union {
    T value_;
    std::function<T()> *function_;
  };
};

}  // namespace esphome
