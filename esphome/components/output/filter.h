#pragma once
#include "esphome/core/optional.h"

namespace esphome {
namespace output {

class BinaryOutput;

class Filter {
 public:
  virtual optional<float> new_value(float value) = 0;
  virtual void initialize(BinaryOutput *parent, Filter *next);

  void input(float value);

  void output(float value);

 protected:
  friend BinaryOutput;

  Filter *next_{nullptr};
  BinaryOutput *parent_{nullptr};
};

class InvertedFilter : public Filter {
 public:
  explicit InvertedFilter(bool inverted);

  optional<float> new_value(float value) override;

 protected:
  bool inverted_;
};

}  // namespace output
}  // namespace esphome
