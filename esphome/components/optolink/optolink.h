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

  static const uint32_t COMMUNICATION_CHECK_WINDOW = 10000;
  static const uint32_t COMMUNICATION_SUSPENSION_DURATION = 10000;
  static const uint32_t MAX_RESPONSE_DELAY = 2000;

 public:
  void setup() override;

  void loop() override;

  size_t write(uint8_t ch) override;

  void set_logger_enabled(bool logger_enabled) { logger_enabled_ = logger_enabled; }
  void set_rx_pin(int rx_pin) { rx_pin_ = rx_pin; }
  void set_tx_pin(int tx_pin) { tx_pin_ = tx_pin; }

  bool write_value(IDatapoint *datapoint, DPValue dp_value);
  bool read_value(IDatapoint *datapoint);

  std::string get_state() { return state_; }

  int get_queue_size();
  bool communication_suspended();
  void notify_receive();
  void notify_send();

 private:
  void set_state_(const char *format, ...);

  void communication_check_();
  void suspend_communication_();
  void resume_communication_();
};

}  // namespace optolink
}  // namespace esphome

#endif
