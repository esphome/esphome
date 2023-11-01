#include "esphome.h"

const uint16_t I2C_ADDRESS = 0x27;
const uint16_t REGISTER_ADDRESS = 0x80;
const uint16_t POLLING_PERIOD = 15000; //milliseconds
char temp = 100; //Initial value of the register

class Krida4chDimmer : public PollingComponent {
 public:
  Krida4chDimmer() : PollingComponent(POLLING_PERIOD) {}
  float get_setup_priority() const override { return esphome::setup_priority::BUS; } //Access I2C bus

  void setup() override {
    //Add code here as needed
    Wire.begin();
    }

  void update() override {
  char register_value = id(dimmer_ch_1).state; //Read the number set on the dashboard
  //Did the user change the input?
  if(register_value != temp){
        Wire.beginTransmission(I2C_ADDRESS);
        Wire.write(REGISTER_ADDRESS);
        Wire.write(register_value);
        Wire.endTransmission();
        temp = register_value; //Swap in the new value
        }
    }
};