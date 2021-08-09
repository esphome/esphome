#pragma once
#include <cstdint>

namespace esphome {

#define LOG_PIN(prefix, pin) \
  if ((pin) != nullptr) { \
    pin->dump_config(); \
  }

enum class GPIOFlags : uint32_t {
  NONE = 0x00,
  INPUT = 0x01,
  OUTPUT = 0x02,
  OPEN_DRAIN = 0x04,
  PULLUP = 0x08,
  PULLDOWN = 0x10,
};

class GPIOFlagsHelper {
 public:
  constexpr GPIOFlagsHelper(GPIOFlags val) : val_(val) {}
  constexpr operator GPIOFlags() const { return val_; }
  constexpr explicit operator bool() const { return static_cast<uint32_t>(val_) != 0; }
 protected:
  GPIOFlags val_;
};
constexpr GPIOFlagsHelper operator&(GPIOFlags lhs, GPIOFlags rhs) {
  return static_cast<GPIOFlags>(
    static_cast<uint32_t>(lhs) &
    static_cast<uint32_t>(rhs)
  );
}
constexpr GPIOFlagsHelper operator|(GPIOFlags lhs, GPIOFlags rhs) {
  return static_cast<GPIOFlags>(
    static_cast<uint32_t>(lhs) |
    static_cast<uint32_t>(rhs)
  );
}

enum class GPIOInterruptType : uint32_t {
  RISING_EDGE = 1,
  FALLING_EDGE = 2,
  ANY_EDGE = 3,
  LOW_LEVEL = 4,
  HIGH_LEVEL = 5,
};


class GPIOPin {
 public:
  virtual void setup() = 0;

  virtual void pin_mode(GPIOFlags flags) = 0;

  virtual bool digital_read() = 0;

  virtual void digital_write(bool value) = 0;

  virtual void dump_config() = 0;
};

/// Copy of GPIOPin that is safe to use from ISRs (with no virtual functions)
class ISRInternalGPIOPin {
 public:
  bool digital_read();
  void digital_write(bool value);
  void clear_interrupt();

 protected:
  friend class InternalGPIOPin;
  ISRInternalGPIOPin(void *arg) : arg_(arg) {}

  const void *arg_;
};

class InternalGPIOPin : public GPIOPin {
 public:
  template<typename T>
  void attach_interrupt(void (*func)(T *), T *arg, GPIOInterruptType type) const {
    this->attach_interrupt_(reinterpret_cast<void (*)(void *)>(func), arg, type);
  }

  virtual void detach_interrupt() const = 0;

  virtual ISRInternalGPIOPin to_isr() const = 0;

 protected:
  virtual void attach_interrupt_(void (*func)(void *), void *arg, GPIOInterruptType type) const = 0;
};

}  // namespace esphome
