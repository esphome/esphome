#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace qmp6988 {

#define QMP6988_U16_t unsigned short
#define QMP6988_S16_t short
#define QMP6988_U32_t unsigned int
#define QMP6988_S32_t int
#define QMP6988_U64_t unsigned long long
#define QMP6988_S64_t long long

/* oversampling */
enum QMP6988Oversampling {
  QMP6988_OVERSAMPLING_SKIPPED = 0x00,
  QMP6988_OVERSAMPLING_1X = 0x01,
  QMP6988_OVERSAMPLING_2X = 0x02,
  QMP6988_OVERSAMPLING_4X = 0x03,
  QMP6988_OVERSAMPLING_8X = 0x04,
  QMP6988_OVERSAMPLING_16X = 0x05,
  QMP6988_OVERSAMPLING_32X = 0x06,
  QMP6988_OVERSAMPLING_64X = 0x07,
};

/* filter */
enum QMP6988IIRFilter {
  QMP6988_IIR_FILTER_OFF = 0x00,
  QMP6988_IIR_FILTER_2X = 0x01,
  QMP6988_IIR_FILTER_4X = 0x02,
  QMP6988_IIR_FILTER_8X = 0x03,
  QMP6988_IIR_FILTER_16X = 0x04,
  QMP6988_IIR_FILTER_32X = 0x05,
};

using qmp6988_cali_data_t = struct Qmp6988CaliData {
  QMP6988_S32_t COE_a0;
  QMP6988_S16_t COE_a1;
  QMP6988_S16_t COE_a2;
  QMP6988_S32_t COE_b00;
  QMP6988_S16_t COE_bt1;
  QMP6988_S16_t COE_bt2;
  QMP6988_S16_t COE_bp1;
  QMP6988_S16_t COE_b11;
  QMP6988_S16_t COE_bp2;
  QMP6988_S16_t COE_b12;
  QMP6988_S16_t COE_b21;
  QMP6988_S16_t COE_bp3;
};

using qmp6988_fk_data_t = struct Qmp6988FkData {
  float a0, b00;
  float a1, a2, bt1, bt2, bp1, b11, bp2, b12, b21, bp3;
};

using qmp6988_ik_data_t = struct Qmp6988IkData {
  QMP6988_S32_t a0, b00;
  QMP6988_S32_t a1, a2;
  QMP6988_S64_t bt1, bt2, bp1, b11, bp2, b12, b21, bp3;
};

using qmp6988_data_t = struct Qmp6988Data {
  uint8_t chip_id;
  uint8_t power_mode;
  float temperature;
  float pressure;
  float altitude;
  qmp6988_cali_data_t qmp6988_cali;
  qmp6988_ik_data_t ik;
};

class QMP6988Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_iir_filter(QMP6988IIRFilter iirfilter);
  void set_temperature_oversampling(QMP6988Oversampling oversampling_t);
  void set_pressure_oversampling(QMP6988Oversampling oversampling_p);

 protected:
  qmp6988_data_t qmp6988_data_;
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};

  QMP6988Oversampling temperature_oversampling_{QMP6988_OVERSAMPLING_16X};
  QMP6988Oversampling pressure_oversampling_{QMP6988_OVERSAMPLING_16X};
  QMP6988IIRFilter iir_filter_{QMP6988_IIR_FILTER_OFF};

  void software_reset_();
  bool get_calibration_data_();
  bool device_check_();
  void set_power_mode_(uint8_t power_mode);
  void write_oversampling_temperature_(unsigned char oversampling_t);
  void write_oversampling_pressure_(unsigned char oversampling_p);
  void write_filter_(unsigned char filter);
  void calculate_pressure_();
  void calculate_altitude_(float pressure, float temp);

  QMP6988_S32_t get_compensated_pressure_(qmp6988_ik_data_t *ik, QMP6988_S32_t dp, QMP6988_S16_t tx);
  QMP6988_S16_t get_compensated_temperature_(qmp6988_ik_data_t *ik, QMP6988_S32_t dt);
};

}  // namespace qmp6988
}  // namespace esphome
