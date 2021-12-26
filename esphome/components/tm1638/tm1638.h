#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"



#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif


#include "esphome/components/binary_sensor/binary_sensor.h"

//#ifdef USE_BINARY_SENSOR
//#include "esphome/components/binary_sensor/binary_sensor.h"
//#endif

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif


namespace esphome {
namespace tm1638 {

class TM1638Component;
#ifdef USE_BINARY_SENSOR
class TM1638Key;
#endif


#ifdef USE_SWITCH
class TM1638Led;
#endif


using tm1638_writer_t = std::function<void(TM1638Component &)>;  //<-   where does this class come from


class TM1638Component : public PollingComponent{
 public:
  void set_writer(tm1638_writer_t &&writer);  //<--  takes in a lambda?
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;
  void set_intensity(uint8_t brightnessLevel);
  void display();

  void set_clk_pin(GPIOPin *pin) { clk_pin_ = pin; }
  void set_dio_pin(GPIOPin *pin) { dio_pin_ = pin; }
  void set_stb_pin(GPIOPin *pin) { stb_pin_ = pin; }


  /// Evaluate the printf-format and print the result at the given position.
  uint8_t printf(uint8_t pos, const char *format, ...) __attribute__((format(printf, 3, 4)));
  /// Evaluate the printf-format and print the result at position 0.
  uint8_t printf(const char *format, ...) __attribute__((format(printf, 2, 3)));

  /// Print `str` at the given position.
  uint8_t print(uint8_t pos, const char *str);
  /// Print `str` at position 0.
  uint8_t print(const char *str);


#ifdef USE_BINARY_SENSOR
  void loop() override;
  uint8_t get_keys();
  void add_tm1638_key(TM1638Key *tm1638_key) { this->tm1638_keys_.push_back(tm1638_key); }
#endif


#ifdef USE_SWITCH
  void add_tm1638_led(TM1638Led *tm1638_led) { this->tm1638_leds_.push_back(tm1638_led); }
#endif


#ifdef USE_TIME
  /// Evaluate the strftime-format and print the result at the given position.
  uint8_t strftime(uint8_t pos, const char *format, time::ESPTime time) __attribute__((format(strftime, 3, 0)));
  /// Evaluate the strftime-format and print the result at position 0.
  uint8_t strftime(const char *format, time::ESPTime time) __attribute__((format(strftime, 2, 0)));
#endif


  //may want to make these protected later
  //Trigger<> *switch1_action_trigger_t{nullptr};
  void setLed(int ledPos, bool ledOnOff);

 protected:
  //void setLed(int ledPos, bool ledOnOff);
  void set7Seg(int segPos, uint8_t segBits);
  void sendCommand(uint8_t value);
  void sendCommandLeaveOpen(uint8_t value);
  void sendCommands(uint8_t commands[], int numCommands);
  void sendCommandSequence(uint8_t commands[], int numCommands, uint8_t startingAddress);
  void shiftOut(uint8_t value);
  void reset(bool onOff);
  void start_();
  void stop_();
  void bit_delay_();

  uint8_t shiftIn();


  // void GetTranslation(char *str);

  uint8_t intensity_{7};  /// brghtness of the display  0 through 7
  GPIOPin *clk_pin_;
  GPIOPin *stb_pin_;
  GPIOPin *dio_pin_;
  uint8_t *buffer_;
  uint8_t buttonState_;  // should this be a pointer?
  optional<tm1638_writer_t> writer_{};

  #ifdef USE_BINARY_SENSOR
    std::vector<TM1638Key *> tm1638_keys_{};
  #endif

  #ifdef USE_SWITCH
    std::vector<TM1638Led *> tm1638_leds_{};
  #endif


};





#ifdef USE_SWITCH


class TM1638Led : public switch_::Switch {
  friend class TM1638Component;


  public:
  void set_lednum(int8_t led_num) { led_num_ = led_num; }


 protected:

  void write_state(bool state) override
  {

   // TM1638Component::setLed(led_num_, state);  //  <---   not sure what to do about this.

    //Should I push out the bits over the wire here, or pass in a reference to the TM1638 object and expose the setLed() method

   // this->TM1638Component::setLed(led_num_, state);

   //tm1638_->setLed(led_num_, state);

   this->publish_state(static_cast<bool>(state));
  }

  uint8_t led_num_{0};

};

#endif

}  // namespace tm1638
}  // namespace esphome
