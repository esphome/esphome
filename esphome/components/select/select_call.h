#pragma once

#include "esphome/core/helpers.h"

namespace esphome::select {

class Select;

enum SelectOperation {
  SELECT_OP_NONE,
  SELECT_OP_SET,
  SELECT_OP_NEXT,
  SELECT_OP_PREVIOUS,
  SELECT_OP_FIRST,
  SELECT_OP_LAST
};

class SelectCall {
 public:
  explicit SelectCall(Select *parent) : parent_(parent) {}
  void perform();

  SelectCall &set_option(const char *option, size_t len);
  SelectCall &set_option(const std::string &option) { return this->set_option(option.data(), option.size()); }
  SelectCall &set_option(const char *option) { return this->set_option(option, strlen(option)); }
  SelectCall &set_index(size_t index);

  SelectCall &select_next(bool cycle);
  SelectCall &select_previous(bool cycle);
  SelectCall &select_first();
  SelectCall &select_last();

  SelectCall &with_operation(SelectOperation operation);
  SelectCall &with_cycle(bool cycle);
  SelectCall &with_option(const char *option, size_t len);
  SelectCall &with_option(const std::string &option) { return this->with_option(option.data(), option.size()); }
  SelectCall &with_option(const char *option) { return this->with_option(option, strlen(option)); }
  SelectCall &with_index(size_t index);

 protected:
  __attribute__((always_inline)) inline optional<size_t> calculate_target_index_(const char *name);

  Select *const parent_;
  optional<size_t> index_;
  SelectOperation operation_{SELECT_OP_NONE};
  bool cycle_;
};

}  // namespace esphome::select
