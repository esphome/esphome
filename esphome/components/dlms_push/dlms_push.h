#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#include <vector>
#include <string>
#include <array>
#include <memory>

namespace esphome {
namespace dlms_push {

class DlmsParser;

class DlmsPushComponent : public Component, public uart::UARTDevice {
 public:
  DlmsPushComponent();

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_receive_timeout(uint32_t timeout_ms) { this->receive_timeout_ms_ = timeout_ms; }
  void set_show_log(bool show_log) { this->show_log_ = show_log; }

#ifdef USE_SENSOR
  void register_sensor(const std::string &obis_code, sensor::Sensor *sensor);
#endif

 protected:
  void read_rx_buffer_();
  void process_frame_();

  void on_data_parsed_(const char *obis_code, float float_val, bool is_numeric);

  uint32_t receive_timeout_ms_{50};
  bool show_log_{false};

  static constexpr size_t MAX_RX_BUFFER_SIZE = 2048;
  std::unique_ptr<uint8_t[]> rx_buffer_;
  size_t rx_buffer_len_{0};
  uint32_t last_rx_char_time_{0};
  bool receiving_{false};
  bool overflow_warned_{false};

  std::unique_ptr<DlmsParser> parser_;

#ifdef USE_SENSOR
  struct NumericSensorEntry {
    std::string obis;
    sensor::Sensor *sensor;
  };
  std::vector<NumericSensorEntry> sensors_;
#endif
};

}  // namespace dlms_push
}  // namespace esphome
