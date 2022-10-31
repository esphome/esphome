#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "filter.h"

#ifdef USE_POWER_SUPPLY
#include "esphome/components/power_supply/power_supply.h"
#endif

namespace esphome {
namespace output {

class BinaryOutput {
 public:
#ifdef USE_POWER_SUPPLY
  /** Use this to connect up a power supply to this output.
   *
   * Whenever this output is enabled, the power supply will automatically be turned on.
   *
   * @param power_supply The PowerSupplyComponent, set this to nullptr to disable the power supply.
   */
  void set_power_supply(power_supply::PowerSupply *power_supply) { this->power_.set_parent(power_supply); }
#endif

  /// Enable or disable this binary output.
  virtual void set_state(bool state) {
    if (this->filter_list_ == nullptr) {
#ifdef USE_POWER_SUPPLY
      if (state) {  // ON
        this->power_.request();
      } else {  // OFF
        this->power_.unrequest();
      }
#endif
      this->write_state(state);
    } else {
      this->filter_list_->input(state ? 1.0f : 0.0f);
    }
  }

  /// Enable this binary output.
  virtual void turn_on() { this->set_state(true); }

  /// Disable this binary output.
  virtual void turn_off() { this->set_state(false); }

  /// Add a filter to the filter chain. Will be appended to the back.
  void add_filter(Filter *filter);

  /** Add a list of vectors to the back of the filter chain.
   *
   * This may look like:
   *
   * output->add_filters({
   *   InvertedFilter(true),
   * });
   */
  void add_filters(const std::vector<Filter *> &filters);

  /// Clear the filters and replace them by filters.
  void set_filters(const std::vector<Filter *> &filters);

  /// Clear the entire filter chain.
  void clear_filters();

 protected:
  friend Filter;

  virtual void write_state(bool state) = 0;
  virtual void write_state(float state);

  Filter *filter_list_{nullptr};

#ifdef USE_POWER_SUPPLY
  power_supply::PowerSupplyRequester power_{};
#endif
};

}  // namespace output
}  // namespace esphome
