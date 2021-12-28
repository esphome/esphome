#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace tm1638 {

class KeyListener {
 public:
  virtual void keys_update(uint8_t keys) {};
};


class TM1638Component;

using tm1638_writer_t = std::function<void(TM1638Component &)>;

class TM1638Component : public PollingComponent{
 public:
  void set_writer(tm1638_writer_t &&writer) { this->writer_ = writer; }
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;
  void set_intensity(uint8_t brightnessLevel);
  void display();

  void set_clk_pin(GPIOPin *pin) { this->clk_pin_ = pin; }
  void set_dio_pin(GPIOPin *pin) { this->dio_pin_ = pin; }
  void set_stb_pin(GPIOPin *pin) { this->stb_pin_ = pin; }

  void register_listener(KeyListener *listener) { this->listeners_.push_back(listener); }


  /// Evaluate the printf-format and print the result at the given position.
  uint8_t printf(uint8_t pos, const char *format, ...) __attribute__((format(printf, 3, 4)));
  /// Evaluate the printf-format and print the result at position 0.
  uint8_t printf(const char *format, ...) __attribute__((format(printf, 2, 3)));

  /// Print `str` at the given position.
  uint8_t print(uint8_t pos, const char *str);
  /// Print `str` at position 0.
  uint8_t print(const char *str);

  void loop() override;
  uint8_t get_keys();

#ifdef USE_TIME
  /// Evaluate the strftime-format and print the result at the given position.
  uint8_t strftime(uint8_t pos, const char *format, time::ESPTime time) __attribute__((format(strftime, 3, 0)));
  /// Evaluate the strftime-format and print the result at position 0.
  uint8_t strftime(const char *format, time::ESPTime time) __attribute__((format(strftime, 2, 0)));
#endif

  void set_led(int ledPos, bool ledOnOff);

 protected:
  void set_7seg_(int segPos, uint8_t segBits);
  void sendCommand(uint8_t value);
  void sendCommandLeaveOpen(uint8_t value);
  void sendCommands(uint8_t commands[], int numCommands);
  void sendCommandSequence(uint8_t commands[], int numCommands, uint8_t startingAddress);
  void shiftOut(uint8_t value);
  void reset(bool onOff);
  uint8_t shiftIn();

  // void GetTranslation(char *str);

  uint8_t intensity_{};  /// brghtness of the display  0 through 7
  GPIOPin *clk_pin_;
  GPIOPin *stb_pin_;
  GPIOPin *dio_pin_;
  uint8_t *buffer_;
  uint8_t buttonState_;  // should this be a pointer?
  optional<tm1638_writer_t> writer_{};

  std::vector<KeyListener *> listeners_{};
};

}  // namespace tm1638
}  // namespace esphome
