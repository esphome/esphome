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
};
class Lora : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void update() override;
  void loop() override;
  void dump_config() override;
  // local
  /// Helper function to read the value of a pin.
  bool digital_read(uint8_t pin);
  /// Helper function to write the value of a pin.
  void digital_write(uint8_t pin, bool value);
  /// Helper function to set the pin mode of a pin.
  void pin_mode(uint8_t pin, gpio::Flags flags);
  void set_message_sensor(text_sensor::TextSensor *s) { message_text_sensor_ = s; }
  void set_rssi_sensor(sensor::Sensor *s) { rssi_sensor_ = s; }
  void set_pin_aux(GPIOPin *s) { pin_aux_ = s; }
  void set_pin_m0(GPIOPin *s) { pin_m0_ = s; }
  void set_pin_m1(GPIOPin *s) { pin_m1_ = s; }

 private:
  ModeType mode_ = MODE_0_NORMAL;
  // set WOR mode
  void set_mode_(ModeType mode);
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
  GPIOPin *pin_aux_{nullptr};
  GPIOPin *pin_m0_{nullptr};
  GPIOPin *pin_m1_{nullptr};
};

}  // namespace lora
}  // namespace esphome
