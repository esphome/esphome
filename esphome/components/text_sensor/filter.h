#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

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

/** Optimized lambda filter for stateless lambdas (no capture).
 *
 * Uses function pointer instead of std::function to reduce memory overhead.
 * Memory: 4 bytes (function pointer on 32-bit) vs 32 bytes (std::function).
 */
class StatelessLambdaFilter : public Filter {
 public:
  explicit StatelessLambdaFilter(optional<std::string> (*lambda_filter)(std::string)) : lambda_filter_(lambda_filter) {}

  optional<std::string> new_value(std::string value) override { return this->lambda_filter_(value); }

 protected:
  optional<std::string> (*lambda_filter_)(std::string);
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
  explicit AppendFilter(const char *suffix) : suffix_(suffix) {}
  optional<std::string> new_value(std::string value) override;

 protected:
  const char *suffix_;
};

/// A simple filter that adds a string to the start of another string
class PrependFilter : public Filter {
 public:
  explicit PrependFilter(const char *prefix) : prefix_(prefix) {}
  optional<std::string> new_value(std::string value) override;

 protected:
  const char *prefix_;
};

struct Substitution {
  const char *from;
  const char *to;
};

/// A simple filter that replaces a substring with another substring
class SubstituteFilter : public Filter {
 public:
  explicit SubstituteFilter(const std::initializer_list<Substitution> &substitutions);
  optional<std::string> new_value(std::string value) override;

 protected:
  FixedVector<Substitution> substitutions_;
};

/** A filter that maps values from one set to another
 *
 * Uses linear search instead of std::map for typical small datasets (2-20 mappings).
 * Linear search on contiguous memory is faster than red-black tree lookups when:
 * - Dataset is small (< ~30 items)
 * - Memory is contiguous (cache-friendly, better CPU cache utilization)
 * - No pointer chasing overhead (tree node traversal)
 * - String comparison cost dominates lookup time
 *
 * Benchmark results (see benchmark_map_filter.cpp):
 * - 2 mappings:  Linear 1.26x faster than std::map
 * - 5 mappings:  Linear 2.25x faster than std::map
 * - 10 mappings: Linear 1.83x faster than std::map
 * - 20 mappings: Linear 1.59x faster than std::map
 * - 30 mappings: Linear 1.09x faster than std::map
 * - 40 mappings: std::map 1.27x faster than Linear (break-even)
 *
 * Benefits over std::map:
 * - ~2KB smaller flash (no red-black tree code)
 * - ~24-32 bytes less RAM per mapping (no tree node overhead)
 * - Faster for typical ESPHome usage (2-10 mappings common, 20+ rare)
 *
 * Break-even point: ~35-40 mappings, but ESPHome configs rarely exceed 20
 */
class MapFilter : public Filter {
 public:
  explicit MapFilter(const std::initializer_list<Substitution> &mappings);
  optional<std::string> new_value(std::string value) override;

 protected:
  FixedVector<Substitution> mappings_;
};

}  // namespace text_sensor
}  // namespace esphome
