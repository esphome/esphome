#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <queue>
#include <utility>
#include <map>

namespace esphome {
namespace prometheus {

class PrometheusHandler;

/** Apply a filter to Prometheus metrics names.
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
  virtual optional<std::string> new_value(std::string value);

  /// Initialize this filter.
  virtual void initialize(PrometheusHandler *parent);

  std::string input(const std::string &value);

 protected:
  friend PrometheusHandler;

  PrometheusHandler *parent_{nullptr};
};

/// A filter that maps values from one set to another
class MapFilter : public Filter {
 public:
  MapFilter(std::map<std::string, std::string> mappings) : mappings_(std::move(mappings)) {}
  optional<std::string> new_value(std::string value) override;

 protected:
  std::map<std::string, std::string> mappings_;
};

}  // namespace prometheus
}  // namespace esphome
