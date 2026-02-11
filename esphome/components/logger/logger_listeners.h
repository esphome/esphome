#pragma once

#include "logger.h"

namespace esphome::logger {

/** Interface for receiving log messages without std::function overhead.
 *
 * Components can implement this interface instead of using lambdas with std::function
 * to reduce flash usage from std::function type erasure machinery.
 *
 * Usage:
 *   class MyComponent : public Component, public LogListener {
 *    public:
 *     void setup() override {
 *       if (logger::global_logger != nullptr)
 *         logger::global_logger->add_log_listener(this);
 *     }
 *     void on_log(uint8_t level, const char *tag, const char *message, size_t message_len) override {
 *       // Handle log message
 *     }
 *   };
 */
class LogListener {
 public:
  virtual void on_log(uint8_t level, const char *tag, const char *message, size_t message_len) = 0;
};

#ifdef USE_LOGGER_LEVEL_LISTENERS
/** Interface for receiving log level changes without std::function overhead.
 *
 * Components can implement this interface instead of using lambdas with std::function
 * to reduce flash usage from std::function type erasure machinery.
 *
 * Usage:
 *   class MyComponent : public Component, public LoggerLevelListener {
 *    public:
 *     void setup() override {
 *       if (logger::global_logger != nullptr)
 *         logger::global_logger->add_logger_level_listener(this);
 *     }
 *     void on_log_level_change(uint8_t level) override {
 *       // Handle log level change
 *     }
 *   };
 */
class LoggerLevelListener {
 public:
  virtual void on_log_level_change(uint8_t level) = 0;
};
#endif

class LoggerMessageTrigger : public Trigger<uint8_t, const char *, const char *>, public LogListener {
 public:
  explicit LoggerMessageTrigger(Logger *parent, uint8_t level) : level_(level) { parent->add_log_listener(this); }

  void on_log(uint8_t level, const char *tag, const char *message, size_t message_len) override {
    (void) message_len;
    if (level <= this->level_) {
      this->trigger(level, tag, message);
    }
  }

 protected:
  uint8_t level_;
};

}  // namespace esphome::logger
