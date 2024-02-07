#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <queue>
#include <utility>
#include <map>
#include <vector>

namespace esphome {
namespace text_sensor {

class TextSensor;

/** Apply a filter to text sensor values such as to_upper.
 *
 * This class is purposefully kept quite simple, since more complicated
 * filters should really be done with the filter sensor in Home Assistant.
 */
class Filter {
 public:
  /** This will be called every time the filter receives a new value.
   *
   * It can return an empty optional to indicate that the filter chain
   * should stop, otherwise the value in the filter will be passed down
   * the chain.
   *
   * @param value The new value.
   * @return An optional string, the new value that should be pushed out.
   */
  virtual optional<std::string> new_value(std::string value) = 0;

  /// Initialize this filter, please note this can be called more than once.
  virtual void initialize(TextSensor *parent, Filter *next);

  void input(const std::string &value);

  void output(const std::string &value);

 protected:
  friend TextSensor;

  Filter *next_{nullptr};
  TextSensor *parent_{nullptr};
};

using lambda_filter_t = std::function<optional<std::string>(std::string)>;

/** This class allows for creation of simple template filters.
 *
 * The constructor accepts a lambda of the form std::string -> optional<std::string>.
 * It will be called with each new value in the filter chain and returns the modified
 * value that shall be passed down the filter chain. Returning an empty Optional
 * means that the value shall be discarded.
 */
class LambdaFilter : public Filter {
 public:
  explicit LambdaFilter(lambda_filter_t lambda_filter);

  optional<std::string> new_value(std::string value) override;

  const lambda_filter_t &get_lambda_filter() const;
  void set_lambda_filter(const lambda_filter_t &lambda_filter);

 protected:
  lambda_filter_t lambda_filter_;
};

/// A simple filter that converts all text to uppercase
class ToUpperFilter : public Filter {
 public:
  optional<std::string> new_value(std::string value) override;
};

/// A simple filter that converts all text to lowercase
class ToLowerFilter : public Filter {
 public:
  optional<std::string> new_value(std::string value) override;
};

/// A simple filter that adds a string to the end of another string
class AppendFilter : public Filter {
 public:
  AppendFilter(std::string suffix) : suffix_(std::move(suffix)) {}
  optional<std::string> new_value(std::string value) override;

 protected:
  std::string suffix_;
};

/// A simple filter that adds a string to the start of another string
class PrependFilter : public Filter {
 public:
  PrependFilter(std::string prefix) : prefix_(std::move(prefix)) {}
  optional<std::string> new_value(std::string value) override;

 protected:
  std::string prefix_;
};

/// A simple filter that replaces a substring with another substring
class SubstituteFilter : public Filter {
 public:
  SubstituteFilter(std::vector<std::string> from_strings, std::vector<std::string> to_strings)
      : from_strings_(std::move(from_strings)), to_strings_(std::move(to_strings)) {}
  optional<std::string> new_value(std::string value) override;

 protected:
  std::vector<std::string> from_strings_;
  std::vector<std::string> to_strings_;
};

/// A filter that maps values from one set to another
class MapFilter : public Filter {
 public:
  MapFilter(std::map<std::string, std::string> mappings) : mappings_(std::move(mappings)) {}
  optional<std::string> new_value(std::string value) override;

 protected:
  std::map<std::string, std::string> mappings_;
};

}  // namespace text_sensor
}  // namespace esphome
