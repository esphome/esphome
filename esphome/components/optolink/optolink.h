#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <VitoWiFi.h>

using namespace esphome;
using namespace sensor;
using namespace binary_sensor;
using namespace text_sensor;

namespace esphome {
namespace optolink {

// '00' ='WW' '01' ='RED' '02' ='NORM' '03' ='H+WW' '04' ='H+WW FS' '05' ='ABSCHALT'

//=====================================================================================================================
class Optolink : public esphome::Component, public Print {
 protected:
  std::string error_ = "OK";
  std::string log_buffer_;
  bool logger_enabled_ = false;
  int rx_pin_;
  int tx_pin_;

  void _comm();

 public:
  void setup() override;

  void loop() override;

  size_t write(uint8_t ch) override;

  void set_logger_enabled(bool logger_enabled) { logger_enabled_ = logger_enabled; }
  void set_rx_pin(int rx_pin) { rx_pin_ = rx_pin; }
  void set_tx_pin(int tx_pin) { tx_pin_ = tx_pin; }

  void write_value(IDatapoint *datapoint, DPValue dpValue);
  void read_value(IDatapoint *datapoint);

  void set_error(const std::string &format, ...);
  std::string get_error() { return error_; }
};

}  // namespace optolink
}  // namespace esphome
