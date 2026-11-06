#include "event.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"

namespace esphome::event {

ESPHOME_LOG_TAG(TAG, "event");

#ifdef USE_EVENT_DATA
void Event::trigger(const char *event_type) { this->trigger(event_type, {}); }

void Event::trigger(const char *event_type, std::initializer_list<EventArg> args) {
#else
void Event::trigger(const char *event_type) {
#endif
  // Linear search with strcmp - faster than std::set for small datasets (1-5 items typical)
  const char *found = nullptr;
  for (const char *type : this->types_) {
    if (strcmp(type, event_type) == 0) {
      found = type;
      break;
    }
  }
  if (found == nullptr) {
    ESP_LOGE(TAG, "'%s': invalid event type for trigger(): %s", LOG_STR_ARG(this->get_log_name()), event_type);
    return;
  }
#ifdef USE_EVENT_DATA
  if (this->data_ != nullptr) {
    this->store_data_(found, args);
  } else if (args.size() != 0) {
    ESP_LOGE(TAG, "'%s': event has no variables", LOG_STR_ARG(this->get_log_name()));
  }
#endif
  this->last_event_type_ = found;
  ESP_LOGV(TAG, "'%s' >> '%s'", LOG_STR_ARG(this->get_log_name()), this->last_event_type_);
  this->event_callback_.call(StringRef(found));
#if defined(USE_EVENT) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_event(this);
#endif
}

void Event::set_event_types_copy_(const char *const *event_types, size_t count) {
#ifdef USE_EVENT_DATA
  if (this->data_ != nullptr && count > EVENT_DATA_MAX_TYPES) {
    ESP_LOGE(TAG, "'%s': an event with variables has at most %u event types", LOG_STR_ARG(this->get_log_name()),
             EVENT_DATA_MAX_TYPES);
    return;
  }
  this->clear_data_();
#endif
  this->types_.assign_copy(event_types, count);
  this->last_event_type_ = nullptr;  // Reset when types change
}

#ifdef USE_EVENT_DATA
// By name, so a variable stays with its event types when set_event_types() reorders or replaces the list
static bool type_may_carry(const char *const *event_types, const char *event_type) {
  for (;; event_types++) {
    const char *name = progmem_read_ptr(event_types);
    if (name == nullptr)
      return false;
    if (strcmp(name, event_type) == 0)
      return true;
  }
}

void Event::store_data_(const char *event_type, std::initializer_list<EventArg> args) {
  EventDataHeader *data = this->data_;
  const EventData reader(data);
  uint16_t &present = data->present;
  uint8_t *values = data->values();
  present = 0;
  for (const EventArg &arg : args) {
    if (arg.index >= data->count) {
      ESP_LOGE(TAG, "'%s': no variable %u", LOG_STR_ARG(this->get_log_name()), arg.index);
      continue;
    }
    const EventVariable variable = reader.variable(arg.index);
    if (variable.event_types != nullptr && !type_may_carry(variable.event_types, event_type)) {
      ESP_LOGE(TAG, "'%s': %s does not carry %s", LOG_STR_ARG(this->get_log_name()), event_type,
               LOG_STR_ARG(variable.name));
      continue;
    }
    uint8_t *value = values + variable.offset;
    const EventValue &given = arg.value;
    if (variable.type == EVENT_VARIABLE_TYPE_BOOL && given.type == EVENT_VARIABLE_TYPE_BOOL) {
      *value = given.bool_value;
    } else if (variable.type == EVENT_VARIABLE_TYPE_INT && given.type == EVENT_VARIABLE_TYPE_INT) {
      std::memcpy(value, &given.int_value, sizeof(given.int_value));
    } else if (variable.type == EVENT_VARIABLE_TYPE_FLOAT &&
               (given.type == EVENT_VARIABLE_TYPE_FLOAT || given.type == EVENT_VARIABLE_TYPE_INT)) {
      const float number =
          given.type == EVENT_VARIABLE_TYPE_FLOAT ? given.float_value : static_cast<float>(given.int_value);
      std::memcpy(value, &number, sizeof(number));
    } else if (variable.type == EVENT_VARIABLE_TYPE_STRING && given.type == EVENT_VARIABLE_TYPE_STRING) {
      // Cutting an id or a hex text would make it wrong, so a long text is left out
      const StringRef &text = given.string_value;
      if (text.size() > variable.max_length) {
        ESP_LOGW(TAG, "'%s': %s is %zu bytes, at most %u", LOG_STR_ARG(this->get_log_name()),
                 LOG_STR_ARG(variable.name), text.size(), variable.max_length);
        continue;
      }
      std::memcpy(value, text.c_str(), text.size());
      value[text.size()] = '\0';
    } else {
      ESP_LOGE(TAG, "'%s': wrong type for %s", LOG_STR_ARG(this->get_log_name()), LOG_STR_ARG(variable.name));
      continue;
    }
    present |= 1 << arg.index;
  }
}
#endif

}  // namespace esphome::event
