#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/progmem.h"
#ifdef USE_TEXT_SENSOR_FILTER
#include "esphome/components/text_sensor/filter.h"
#endif

#include <initializer_list>
#include <memory>

namespace esphome::text_sensor {

class TextSensor;

void log_text_sensor(const char *tag, const char *prefix, const char *type, TextSensor *obj);

#define LOG_TEXT_SENSOR(prefix, type, obj) log_text_sensor(TAG, prefix, LOG_STR_LITERAL(type), obj)

#define SUB_TEXT_SENSOR(name) \
 protected: \
  text_sensor::TextSensor *name##_text_sensor_{nullptr}; \
\
 public: \
  void set_##name##_text_sensor(text_sensor::TextSensor *text_sensor) { this->name##_text_sensor_ = text_sensor; }

class TextSensor : public EntityBase {
 public:
  std::string state;

  // User provided, not "= default": `new(p) TextSensor()` would zero-fill .bss that is already zero.
  TextSensor() {}
  ~TextSensor() = default;

  /// Getter-syntax for .state.
  const std::string &get_state() const;
  /// Returns the raw (pre-filter) state.
  const std::string &get_raw_state() const;

  void publish_state(const std::string &state) { this->publish_state(state.data(), state.size()); }
  void publish_state(const char *state) { this->publish_state(state, strlen(state)); }
  void publish_state(const char *state, size_t len);
  /// Publish past the filters, as both the raw and the filtered state. The filters keep their state, so a value they
  /// hold or a timeout they armed can still be sent later.
  void publish_state_unfiltered(const char *state, size_t len);
  void publish_state_unfiltered(const char *state) { this->publish_state_unfiltered(state, strlen(state)); }
  /// Run the raw state through the filters again, without calling the raw state callbacks; after
  /// publish_state_unfiltered(), this gives the filtered form of a value that will not be sent again.
  /// Inline, so its string copy is only built into the components that call it.
  void reapply_filters() {
#ifdef USE_TEXT_SENSOR_FILTER
    if (this->filter_list_ != nullptr)
      this->filter_list_->input(this->raw_state_);
#endif
  }
#ifdef USE_ESP8266
  /// Publish a flash string, such as a PROGMEM_STRING_TABLE get_progmem_str(); at most 255 characters, the Home
  /// Assistant state limit, are kept.
  void publish_state(ProgmemStr state);
#endif

#ifdef USE_TEXT_SENSOR_FILTER
  /// Add a filter to the filter chain. Will be appended to the back.
  void add_filter(Filter *filter);

  /// Add a list of vectors to the back of the filter chain.
  void add_filters(std::initializer_list<Filter *> filters);

  /// Clear the filters and replace them by filters.
  void set_filters(std::initializer_list<Filter *> filters);

  /// Clear the entire filter chain.
  void clear_filters();
#endif

  template<typename F> void add_on_state_callback(F &&callback) { this->callback_.add(std::forward<F>(callback)); }
  /// Add a callback that will be called every time the sensor sends a raw value.
  /// When USE_TEXT_SENSOR_FILTER is not enabled, delegates to the regular callback
  /// since raw state equals filtered state without filter support compiled in.
  template<typename F> void add_on_raw_state_callback(F &&callback) {
#ifdef USE_TEXT_SENSOR_FILTER
    this->raw_callback_.add(std::forward<F>(callback));
#else
    this->callback_.add(std::forward<F>(callback));
#endif
  }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)

  void internal_send_state_to_frontend(const std::string &state) {
    this->internal_send_state_to_frontend(state.data(), state.size());
  }
  void internal_send_state_to_frontend(const char *state, size_t len);

 protected:
#ifdef USE_TEXT_SENSOR_FILTER
  /// Stores the raw state and calls the raw state callbacks; inlined so publish_state() stays as small as before
  inline void ESPHOME_ALWAYS_INLINE set_raw_state_(const char *state, size_t len) {
    // Only assign if changed to avoid heap allocation
    if (len != this->raw_state_.size() || memcmp(state, this->raw_state_.data(), len) != 0) {
      this->raw_state_.assign(state, len);
    }
    this->raw_callback_.call(this->raw_state_);
  }
#endif
  /// Notify frontend that state has changed (assumes this->state is already set)
  void notify_frontend_();
#ifdef USE_TEXT_SENSOR_FILTER
  std::string raw_state_;  ///< Backing storage for the raw (pre-filter) value. Only used when a filter is attached.
  LazyCallbackManager<void(const std::string &)> raw_callback_;  ///< Storage for raw state callbacks.
#endif
  LazyCallbackManager<void(const std::string &)> callback_;  ///< Storage for filtered state callbacks.

#ifdef USE_TEXT_SENSOR_FILTER
  Filter *filter_list_{nullptr};  ///< Store all active filters.
#endif
};

}  // namespace esphome::text_sensor
