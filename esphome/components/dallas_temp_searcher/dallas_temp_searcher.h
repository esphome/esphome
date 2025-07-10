#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/dallas_temp/dallas_temp.h"
#include <vector>
#include "esphome/components/groups/groups.h"

namespace esphome {
namespace dallas_temp_searcher {

enum class SearchMode : uint8_t { ALL = 0, ADDRESS_MAP = 1 };

// Class for dynamic creation DallasTemperatureSensor for sensors connected to onewire bus
class DallasTemperatureSearcher : public Component, public one_wire::OneWireDevice, public groups::GroupsStorage {
 public:
  void setup() override;

  // setup should be called before setup dallas temp sensors
  float get_setup_priority() const override { return setup_priority::DATA + 1; }

  // Update interval for all sensors
  void set_update_interval(uint32_t update_interval_ms) { this->update_interval_ms_ = update_interval_ms; }

  void dump_config() override;

  uint16_t sensors_size() { return this->sensors_.size(); }

  dallas_temp::DallasTemperatureSensor *sensor(uint16_t number) {
    if (number > this->sensors_.size() || number == 0)
      return nullptr;
    return this->sensors_[number - 1];
  }

  void set_search_mode(SearchMode mode) { this->search_mode_ = mode; }

  void set_max_sensors_num(size_t max_num) { this->max_sensors_num_ = max_num; }

  void set_name_prefix(const char *prefix) { this->name_prefix_ = StringRef(prefix); }

  void set_name_start_addr_byte(uint8_t byte) { this->name_start_address_ = byte; }
  void set_name_stop_addr_byte(uint8_t byte) { this->name_stop_address_ = byte; }

 protected:
  void set_default_parameters_(dallas_temp::DallasTemperatureSensor *sensor);
  void restore_sensors_count_();
  bool restore_address_data_(ESPPreferenceObject &obj);

  dallas_temp::DallasTemperatureSensor *make_sensor_base_(const uint64_t &address, const EntityBaseInfo &info);
  dallas_temp::DallasTemperatureSensor *make_sensor_with_address_(const uint64_t &address);
  dallas_temp::DallasTemperatureSensor *make_sensor_with_number_(const uint64_t &address, uint32_t number);

  std::vector<dallas_temp::DallasTemperatureSensor *> sensors_;
  std::vector<EntityBaseInfo> sensors_params_;
  uint32_t update_interval_ms_ = 60000;
  uint8_t max_sensors_num_;

  uint8_t saved_sensors_num_ = 0;

  std::vector<uint64_t> saved_addresses_;

  SearchMode search_mode_ = SearchMode::ALL;
  ESPPreferenceObject sensors_count_pref_;
  std::vector<ESPPreferenceObject> addresses_pref_;
  StringRef name_prefix_;

  uint8_t name_start_address_ = 1;
  uint8_t name_stop_address_ = 8;
};

}  // namespace dallas_temp_searcher
}  // namespace esphome
