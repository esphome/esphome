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

class RangeFilter : public Filter {
 public:
  explicit RangeFilter(float min_power, float max_power, bool zero_means_zero);

  optional<float> new_value(float value) override;

 protected:
  float min_power_;
  float max_power_;
  bool zero_means_zero_;
};

using lambda_filter_t = std::function<optional<float>(float)>;

class LambdaFilter : public Filter {
 public:
  explicit LambdaFilter(lambda_filter_t lambda_filter);

  optional<float> new_value(float value) override;

 protected:
  lambda_filter_t lambda_filter_;
};

}  // namespace output
}  // namespace esphome
