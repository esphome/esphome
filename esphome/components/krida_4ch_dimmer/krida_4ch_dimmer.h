#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

const uint16_t I2C_ADDRESS = 0x27;
const uint16_t REGISTER_ADDRESS = 0x80;
const uint16_t POLLING_PERIOD = 15000; //milliseconds
char temp = 100; //Initial value of the register

namespace esphome {
  class Krida4chDimmer : public PollingComponent {
  public:
    Krida4chDimmer() : PollingComponent(POLLING_PERIOD), i2c::I2CDevice() {}
    float get_setup_priority() const override { return esphome::setup_priority::BUS; } //Access I2C bus

    // void setup() override {
    //   //Add code here as needed
      
    //   }

    void update() override {
    char register_value = id(dimmer_ch_1).state; //Read the number set on the dashboard
    //Did the user change the input?
    if(register_value != temp){
        ESP_LOGI("Updating dimmer value...")
        this->write_register(REGISTER_ADDRESS, temp, 1)
        temp = register_value; //Swap in the new value
          }
      }
  };
}