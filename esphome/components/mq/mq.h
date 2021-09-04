#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <map>

namespace esphome {
namespace mq {

enum MQModel {
  MQ_MODEL_2 = 0,
  MQ_MODEL_3,
  MQ_MODEL_4,
  MQ_MODEL_5,
  MQ_MODEL_6,
  MQ_MODEL_7,
  MQ_MODEL_8,
  MQ_MODEL_9,
  MQ_MODEL_135,
};

enum MQGasType {
  MQ_GAS_TYPE_ACETONA,
  MQ_GAS_TYPE_ALCOHOL,
  MQ_GAS_TYPE_BENZENE,
  MQ_GAS_TYPE_CH4,
  MQ_GAS_TYPE_CL2,
  MQ_GAS_TYPE_CO,
  MQ_GAS_TYPE_CO2,
  MQ_GAS_TYPE_ETHANOL,
  MQ_GAS_TYPE_H2,
  MQ_GAS_TYPE_HEXANE,
  MQ_GAS_TYPE_HYDROGEN,
  MQ_GAS_TYPE_ISO_BUTANO,
  MQ_GAS_TYPE_LPG,
  MQ_GAS_TYPE_NH4,
  MQ_GAS_TYPE_NOX,
  MQ_GAS_TYPE_O3,
  MQ_GAS_TYPE_PROPANE,
  MQ_GAS_TYPE_SMOKE,
  MQ_GAS_TYPE_TOLUENO,
};

struct MQGasSensor {
  MQGasType gas_type;
  float a;
  float b;
  sensor::Sensor *sensor;

  std::string gas_name() {
    switch (gas_type) {
      case MQ_GAS_TYPE_ACETONA:
        return "ACETONA";
      case MQ_GAS_TYPE_ALCOHOL:
        return "ALCOHOL";
      case MQ_GAS_TYPE_BENZENE:
        return "BENZENE";
      case MQ_GAS_TYPE_CH4:
        return "CH4";
      case MQ_GAS_TYPE_CL2:
        return "CL2";
      case MQ_GAS_TYPE_CO:
        return "CO";
      case MQ_GAS_TYPE_CO2:
        return "CO2";
      case MQ_GAS_TYPE_ETHANOL:
        return "ETHANOL";
      case MQ_GAS_TYPE_H2:
        return "H2";
      case MQ_GAS_TYPE_HEXANE:
        return "HEXANE";
      case MQ_GAS_TYPE_HYDROGEN:
        return "HYDROGEN";
      case MQ_GAS_TYPE_ISO_BUTANO:
        return "ISO_BUTANO";
      case MQ_GAS_TYPE_LPG:
        return "LPG";
      case MQ_GAS_TYPE_NH4:
        return "NH4";
      case MQ_GAS_TYPE_NOX:
        return "NOX";
      case MQ_GAS_TYPE_O3:
        return "O3";
      case MQ_GAS_TYPE_PROPANE:
        return "PROPANE";
      case MQ_GAS_TYPE_SMOKE:
        return "SMOKE";
      case MQ_GAS_TYPE_TOLUENO:
        return "TOLUENO";
      default:
        return "UNKOWN GAS";
    }
  }
};

struct MQModelParameters {
  MQModel model;
  std::vector<MQGasSensor> gas_sensors;

  std::string model_name() {
    switch (model) {
      case MQ_MODEL_2:
        return "MQ2";
      case MQ_MODEL_3:
        return "MQ3";
      case MQ_MODEL_4:
        return "MQ4";
      case MQ_MODEL_5:
        return "MQ5";
      case MQ_MODEL_6:
        return "MQ6";
      case MQ_MODEL_7:
        return "MQ7";
      case MQ_MODEL_8:
        return "MQ8";
      case MQ_MODEL_9:
        return "MQ9";
      case MQ_MODEL_135:
        return "MQ135";
      default:
        return "MQ Unknown";
    }
  }

  float get_ratio_in_clean_air() {
    switch (model) {
      case MQ_MODEL_2:
        return 9.83;
      case MQ_MODEL_3:
        return 60.0;
      case MQ_MODEL_4:
        return 4.4;
      case MQ_MODEL_5:
        return 6.5;
      case MQ_MODEL_6:
        return 10.0;
      case MQ_MODEL_7:
        return 27.5;
      case MQ_MODEL_8:
        return 70.0;
      case MQ_MODEL_9:
        return 9.6;
      case MQ_MODEL_135:
        return 3.6;
      default:
        return 0.0;
    }
  }
};

class MQSensor : public PollingComponent {
 public:
  MQSensor(GPIOPin *pin, MQModel model, bool is_esp8266, float rl);
  void add_sensor(sensor::Sensor *sensor, MQGasType gas_type);
  void set_r0(float r0);

  float get_setup_priority() const override;
  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;

 protected:
  GPIOPin *pin_;
  bool is_esp8266_;
  float rl_ = 10.0;
  float r0_ = 0.0;
  float sensor_volt_ = 0.0;
  float a_ = 0.0;
  float b_ = 0.0;
  uint8_t adc_bit_resolution_ = 10;
  MQModelParameters model_parameters_;

 private:
  void update_sensor_voltage();
  float read_sensor();
  float calibrate(float ratio_in_clean_air);
  void set_a(float a);
  void set_b(float b);
};
}  // namespace mq
}  // namespace esphome
