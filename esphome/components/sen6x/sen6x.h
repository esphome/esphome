#pragma once

#include "esphome/core/component.h"
#include "esphome/core/optional.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

namespace esphome::sen6x {

// The NOx algorithm requires std_initial to stay at 50 (Sensirion datasheet)
static constexpr uint16_t NOX_STD_INITIAL = 50;

// Raw parameter block for the VOC/NOx algorithm tuning commands
struct GasTuning {
  uint16_t index_offset;
  uint16_t learning_time_offset_hours;
  uint16_t learning_time_gain_hours;
  uint16_t gating_max_duration_minutes;
  uint16_t std_initial;
  uint16_t gain_factor;
};

class SEN6XComponent final : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_4_0)
  SUB_SENSOR(pm_10_0)
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(voc)
  SUB_SENSOR(nox)
  SUB_SENSOR(co2)
  SUB_SENSOR(hcho)

 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  enum Sen6xType { SEN62, SEN63C, SEN65, SEN66, SEN68, SEN69C, UNKNOWN };

  void set_type(const std::string &type) { sen6x_type_ = infer_type_from_product_name_(type); }
  void set_voc_algorithm_tuning(uint16_t index_offset, uint16_t learning_time_offset_hours,
                                uint16_t learning_time_gain_hours, uint16_t gating_max_duration_minutes,
                                uint16_t std_initial, uint16_t gain_factor) {
    this->voc_tuning_params_ = GasTuning{
        index_offset, learning_time_offset_hours, learning_time_gain_hours, gating_max_duration_minutes, std_initial,
        gain_factor};
  }
  void set_nox_algorithm_tuning(uint16_t index_offset, uint16_t learning_time_offset_hours,
                                uint16_t learning_time_gain_hours, uint16_t gating_max_duration_minutes,
                                uint16_t gain_factor) {
    this->nox_tuning_params_ = GasTuning{index_offset,
                                         learning_time_offset_hours,
                                         learning_time_gain_hours,
                                         gating_max_duration_minutes,
                                         NOX_STD_INITIAL,
                                         gain_factor};
  }
  void set_automatic_self_calibration(bool enabled) { this->co2_asc_ = enabled; }
  void set_altitude_compensation(uint16_t altitude) { this->altitude_compensation_ = altitude; }
  void set_ambient_pressure_compensation(uint16_t pressure_hpa) { this->ambient_pressure_ = pressure_hpa; }
  void set_ambient_pressure_source(sensor::Sensor *pressure) { this->ambient_pressure_source_ = pressure; }

 protected:
  Sen6xType infer_type_from_product_name_(const std::string &product_name);
  void run_next_setup_step_();
  void finish_setup_();
  bool write_config_words_(uint16_t i2c_command, const uint16_t *data, uint8_t len);
  bool write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning);
  bool write_setup_register_(uint16_t i2c_command, uint16_t value);
  bool update_ambient_pressure_compensation_(float pressure_hpa);
  void poll_data_ready_();
  void read_measurements_();
  void parse_and_publish_measurements_();

  optional<GasTuning> voc_tuning_params_;
  optional<GasTuning> nox_tuning_params_;
  optional<bool> co2_asc_;
  optional<uint16_t> altitude_compensation_;
  optional<uint16_t> ambient_pressure_;
  // Last pressure written to the device, used to skip redundant writes
  optional<uint16_t> last_ambient_pressure_;
  sensor::Sensor *ambient_pressure_source_{nullptr};
  bool pressure_range_warned_{false};
  uint8_t setup_step_index_{0};
  bool initialized_{false};
  std::string product_name_;
  Sen6xType sen6x_type_{UNKNOWN};
  std::string serial_number_;
  uint16_t read_cmd_{0};
  uint8_t firmware_version_major_{0};
  uint8_t firmware_version_minor_{0};
  uint8_t poll_retries_remaining_{0};
  uint8_t read_words_{0};
  bool startup_complete_{false};
};

}  // namespace esphome::sen6x
