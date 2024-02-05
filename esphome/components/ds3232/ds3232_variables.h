#pragma once

#include <string>
#include "ds3232.h"
#include "esphome/core/log.h"
#include "esphome/components/logger/logger.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ds3232 {
namespace ds3232_variables {

static const char *const TAG = "ds3232_nvram";

template<typename T> class DS3232Variable : public Component {
 public:
  using value_type = T;

  explicit DS3232Variable(DS3232Component *parent, const uint8_t reg_id) {
    reg_ = reg_id;
    parent_ = parent;
  }

  explicit DS3232Variable(DS3232Component *parent, const uint8_t reg_id, T initial_value) : initial_value_(initial_value) {
    reg_ = reg_id;
    parent_ = parent;
  }

  /// @brief Access to variable value.
  /// @return Variable
  T &value() { return this->value_; }

  void setup() override {
    if (this->parent_ == nullptr) {
      this->mark_failed_();
      return;
    }

    this->parent_->add_on_variable_init_required_callback([=]() { this->set_to_default_(); });
    this->parent_->add_on_variable_fail_callback([=]() { this->mark_failed_(); });

    if (!this->parent_->is_valid_nvram(this->reg_, sizeof(T))) {
      this->mark_failed_();
      return;
    }

    DS3232NVRAMState nvram_state_ = this->parent_->get_nvram_state();

    while(nvram_state_ != DS3232NVRAMState::OK)
    {
      if(nvram_state_ == DS3232NVRAMState::FAIL)
      {
        this->log_error_("Unable to init with failed NVRAM.");
        this->mark_failed();
        break;
      }
      if(nvram_state_ == DS3232NVRAMState::NEED_RESET)
      {
        this->set_to_default_();
      }
      delay_microseconds_safe(10);
      nvram_state_ = this->parent_->get_nvram_state();
    }

    if(!this->try_read_()) {
      this->mark_failed_();
    } else {
      this->is_initialized_ = true;
    }
  }

  float get_setup_priority() const override { return this->parent_->get_setup_priority() - 0.5f; } //Ensure that variables will be initialized after ds3232 component.

  void loop() override {
    if(!this->try_write_())
      this->mark_failed_();
  }

  void on_shutdown() override { this->try_write_(); }

  bool is_initialized() { return this->is_initialized_; }

 protected:
  uint8_t reg_;
  T initial_value_{};
  T value_{};
  T prev_value_{};
  bool need_init_{false};
  bool need_to_wait_{false};
  bool is_initialized_{false};
  std::va_list llist_{};

  void log_error_(const char* log_str) {
    #ifdef USE_LOGGER
      auto *log = logger::global_logger;
      if (log == nullptr)
        return;

      log->log_vprintf_(ESPHOME_LOG_LEVEL_ERROR, TAG, __LINE__, log_str, this->llist_);
    #endif
  }

  void mark_failed_() {
    this->need_to_wait_ = false;
    this->need_init_ = false;
    this->is_initialized_ = false;
    this->mark_failed();
  }

  void set_to_default_() {
    std::vector<uint8_t> buffer;
    buffer.resize(sizeof(T));
    memcpy(buffer.data(), &this->initial_value_, sizeof(T));
    if(!this->parent_->write_memory(this->reg_, buffer))
    {
      this->mark_failed_();
    } else {
      memcpy(&this->value_, &this->initial_value_, sizeof(T));
      memcpy(&this->prev_value_, &this->initial_value_, sizeof(T));
    };
  }

  bool try_write_() {
    if (this->parent_ == nullptr) {
      return false;
    }
    if (this->parent_->get_nvram_state() == ds3232::DS3232NVRAMState::FAIL)
      return false;

    int diff = memcmp(&this->value_, &this->prev_value_, sizeof(T));
    if (diff != 0) {
      std::vector<uint8_t> buffer;
      buffer.resize(sizeof(T));
      memcpy(buffer.data(), &this->value_, sizeof(T));
      if (!this->parent_->write_memory(this->reg_, buffer)) {
        this->mark_failed_();
        this->log_error_("Unable to write new value to NVRAM.");
        return false;
      } else {
        memcpy(&this->prev_value_, &this->value_, sizeof(T));
        return true;
      };
    } else {
      return true;
    };
    return false;
  }

  bool try_read_() {
    if (this->parent_ == nullptr) {
      this->log_error_("No crystal defined. Read from NVRAM failed.");
      return false;
    }
    if (this->parent_->get_nvram_state() != ds3232::DS3232NVRAMState::OK)
    {
      this->log_error_("Invalid NVRAM state. Unable to read data.");
      return false;
    }
    std::vector<uint8_t> buffer;
    buffer.resize(sizeof(T));
    if (this->parent_->read_memory(this->reg_, buffer)) {
      memcpy(&this->value_, buffer.data(), buffer.size());
      memcpy(&this->prev_value_, &this->value_, sizeof(T));
      return true;
    }
    this->log_error_("Failed to read data.");
    return false;
  }

  DS3232Component *parent_{nullptr};
};

template<typename T> T &id(DS3232Variable<T> *value) {
   value->try_read_();
   return value->value();
}

template<class C, typename... Ts> class DS3232VariableSetAction : public Action<Ts...> {
 public:
  explicit DS3232VariableSetAction(C *parent) : parent_(parent) {}

  using T = typename C::value_type;

  TEMPLATABLE_VALUE(T, value);

  void play(Ts... x) override {
     this->parent_->value() = this->value_.value(x...);
  }

 protected:
  C *parent_;
};

}  // namespace ds3232_variables
}  // namespace ds3232
}  // namespace esphome
