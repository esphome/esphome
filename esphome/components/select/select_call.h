#pragma once

#include "esphome/core/helpers.h"

namespace esphome {
namespace select {

class Select;

enum SelectOperation {
  SELECT_OP_NONE,
  SELECT_OP_SET,
  SELECT_OP_SET_INDEX,
  SELECT_OP_NEXT,
  SELECT_OP_PREVIOUS,
  SELECT_OP_FIRST,
  SELECT_OP_LAST
};

class SelectCall {
 public:
  explicit SelectCall(Select *parent) : parent_(parent) {}
  void perform();

  SelectCall &set_option(const std::string &option);
  SelectCall &set_index(size_t index);

  SelectCall &select_next(bool cycle);
  SelectCall &select_previous(bool cycle);
  SelectCall &select_first();
  SelectCall &select_last();

  SelectCall &with_operation(SelectOperation operation);
  SelectCall &with_cycle(bool cycle);
  SelectCall &with_option(const std::string &option);
  SelectCall &with_index(size_t index);

 protected:
  Select *const parent_;
  optional<std::string> option_;
  optional<size_t> index_;
  SelectOperation operation_{SELECT_OP_NONE};
  bool cycle_;
};

}  // namespace select
}  // namespace esphome
