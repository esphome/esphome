#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace tm1637 {

  //using tm1637_writer_t = std::function<void(TM1637Compontent &)>;

  class TM1637Display : public PollingComponent {
  public:
    TM1637Display(GPIOPin *clk, GPIOPin *dio, uint32_t update_interval = 1000);
    float get_setup_priority() const override;
    //void set_writer(tm1637_writer_t &&writer);

    void setSegments(const uint8_t segments[], uint8_t length = 4, uint8_t pos = 0);
    void setBrightness(uint8_t brightness);
    void clear();
    void showNumberDec(int num, bool leading_zero = false, uint8_t length = 4, uint8_t pos = 0);
    void showNumberDecEx(int num, uint8_t dots = 0, bool leading_zero = false, uint8_t length = 4, uint8_t pos = 0);
    void showNumberHexEx(uint16_t num, uint8_t dots = 0, bool leading_zero = false, uint8_t length = 4, uint8_t pos = 0);
    uint8_t encodeDigit(uint8_t digit);

  protected:
    void setup_pins_();

    void bitDelay();

    void start();
    void stop();

    bool writeByte(uint8_t b);

    void showDots(uint8_t dots, uint8_t* digits);
    void showNumberBaseEx(int8_t base, uint16_t num, uint8_t dots = 0, bool leading_zero = false, uint8_t length = 4, uint8_t pos = 0);
    
    GPIOPin *dio_pin_;
    GPIOPin *clk_pin_;

  private:
    uint8_t m_brightness;
    unsigned int m_bitDelay; 
  };

}  // namespace tm1637
}  // namespace esphome
