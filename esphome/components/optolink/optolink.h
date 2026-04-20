#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class Optolink : public esphome::Component, public Print {
 protected:
  std::string state_ = "initializing";
  std::string log_buffer_;
  bool logger_enabled_ = false;
  int rx_pin_;
  int tx_pin_;
  uint32_t timestamp_loop_ = 0;
  uint32_t timestamp_disruption_ = 0;
  uint32_t timestamp_receive_ = 0;
  uint32_t timestamp_send_ = 0;
  uint32_t communication_suspension_ = 20000;
  uint32_t max_response_delay_ = 2000;

  static const uint32_t COMMUNICATION_CHECK_WINDOW = 10000;

 public:
  void setup() override;

  void loop() override;

  size_t write(uint8_t ch) override;

  void set_logger_enabled(bool logger_enabled) { logger_enabled_ = logger_enabled; }
  void set_rx_pin(int rx_pin) { rx_pin_ = rx_pin; }
  void set_tx_pin(int tx_pin) { tx_pin_ = tx_pin; }
  void set_communication_suspension(uint32_t communication_suspension) {
    communication_suspension_ = communication_suspension;
  }
  void set_max_response_delay(uint32_t set_max_response_delay) { max_response_delay_ = set_max_response_delay; }

  bool write_datapoint(IDatapoint *datapoint, DPValue dp_value);
  bool read_datapoint(IDatapoint *datapoint);

  std::string get_state() { return state_; }

  int get_queue_size();
  bool communication_suspended();
  void notify_receive();
  void notify_send();

 private:
  void set_state_(const char *state);

  void communication_check_();
  void suspend_communication_();
  void resume_communication_();
};

}  // namespace optolink
}  // namespace esphome

#endif
