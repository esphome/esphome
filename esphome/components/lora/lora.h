#pragma once
#include <utility>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lora {

static const char *const TAG = "lora";
static const uint8_t MAX_SIZE_TX_PACKET = 200;
// the mode the receiver is in
enum ModeType {
  MODE_0_NORMAL = 0,
  MODE_0_TRANSMISSION = 0,
  MODE_1_WOR_TRANSMITTER = 1,
  MODE_1_WOR = 1,
  MODE_2_WOR_RECEIVER = 2,
  MODE_2_POWER_SAVING = 2,
  MODE_3_CONFIGURATION = 3,
  MODE_3_PROGRAM = 3,
  MODE_3_SLEEP = 3,
  MODE_INIT = 0xFF
};
class Lora : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void update() override;
  void loop() override;
  void dump_config() override;
  /// Helper function to write the value of a pin.
  void digital_write(uint8_t pin, bool value);
  void set_message_sensor(text_sensor::TextSensor *message_text_sensor) { message_text_sensor_ = message_text_sensor; }
  void set_rssi_sensor(sensor::Sensor *rssi_sensor) { rssi_sensor_ = rssi_sensor; }
  void set_pin_aux(GPIOPin *pin_aux) { pin_aux_ = pin_aux; }
  void set_pin_m0(GPIOPin *pin_m0) { pin_m0_ = pin_m0; }
  void set_pin_m1(GPIOPin *pin_m1) { pin_m1_ = pin_m1; }

 private:
  ModeType mode_ = MODE_INIT;
  // set WOR mode
  void set_mode_(ModeType mode);
  ModeType get_mode_();
  // checks the aux port to see if it is done setting
  void setup_wait_response_(uint32_t timeout = 1000);
  bool can_send_message_();
  void send_pin_info_(uint8_t pin, bool value);

 protected:
  bool update_needed_ = false;
  int rssi_ = 0;
  float latitude_ = -1;
  float longitude_ = -1;
  uint32_t starting_to_check_;
  uint32_t time_out_after_;
  std::string raw_message_;
  text_sensor::TextSensor *message_text_sensor_{nullptr};
  sensor::Sensor *rssi_sensor_{nullptr};
  GPIOPin *pin_aux_;
  GPIOPin *pin_m0_;
  GPIOPin *pin_m1_;
};

}  // namespace lora
}  // namespace esphome
