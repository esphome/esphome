#include "pcf8583.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pcf8583 {

static const char *const TAG = "pcf8583.sensor";

static const uint8_t PCF8583_LOCATION_CONTROL = 0x00;
/*static const uint8_t PCF8583_LOCATION_MILLISECONDS = 0x01;
static const uint8_t PCF8583_LOCATION_SECONDS = 0x02;
static const uint8_t PCF8583_LOCATION_MINUTES = 0x03;
static const uint8_t PCF8583_LOCATION_HOURS = 0x04;
static const uint8_t PCF8583_LOCATION_DAY = 0x05;                       //Address Registers maybe needed for future addition of features
static const uint8_t PCF8583_LOCATION_MONTH = 0x06;
static const uint8_t PCF8583_LOCATION_YEAR = 0x07;
*/

//static const uint8_t PCF8583_LOCATION_ALARM_CONTROL = 0x08;
//static const uint8_t PCF8583_LOCATION_ALARM_SECONDS = 0x10;

static const uint8_t PCF8583_LOCATION_COUNTER = 0x01;

//static const uint8_t PCF8583_LOCATION_OFFSET_YEAR = 0x10;
//static const uint8_t PCF8583_LOCATION_LAST_YEAR = 0x11;

//Function modes
//static const uint8_t PCF8583_MODE_CLOCK_32KHZ = 0x00;
//static const uint8_t PCF8583_MODE_CLOCK_50HZ = 0x10;
static const uint8_t PCF8583_MODE_EVENT_COUNTER = 0x20;
//static const uint8_t PCF8583_MODE_TEST = 0x30;



 void PCF8583Component::setup()  {
    ESP_LOGCONFIG(TAG, "Setting up PCF8583...");
    this->set_to_counter_mode();
}

void PCF8583Component:: dump_config()  {
  ESP_LOGCONFIG(TAG, "PCF8583:");
  LOG_I2C_DEVICE(this);

}



void PCF8583Component::update()  {
    this->update_and_reset();
}

//pushing state to esphome and resetting the counter
void PCF8583Component::update_and_reset()  {
  this->counter_->publish_state(read_counter_());
  this->reset_counter_();
}


float PCF8583Component::get_setup_priority() const { return setup_priority::DATA; }


//reading the counter register as BCD format
uint32_t PCF8583Component::read_counter_()  {          
  uint8_t data[3];
  this->read_bytes(PCF8583_LOCATION_COUNTER, data , 3);
  unsigned long count = 0;
  count = bcd2byte_(data[0]);
  count = count + bcd2byte_(data[1]) * 100L;
  count = count + bcd2byte_(data[2]) * 10000L;

  return count;
}

//reset counter; set_counter could be implemented the same, but I don´t see a usecase
void PCF8583Component::reset_counter_(){
  uint8_t data[3];
  unsigned long count = 0;
  data[0] = byte2bcd_(count % 100);
  data[1] = byte2bcd_((count / 100) % 100);
  data[2] = byte2bcd_((count / 10000) % 100);
  this->write_bytes(PCF8583_LOCATION_COUNTER, data, 3);
}



//setting up the PCFComponent as a counter
void PCF8583Component::set_to_counter_mode()  {
  this->write_byte(PCF8583_LOCATION_CONTROL,PCF8583_MODE_EVENT_COUNTER);
}



//converting bcd value to byte
uint8_t PCF8583Component::bcd2byte_(uint8_t value)  {
  return ((value >> 4) * 10) + (value & 0x0f);
}

//converting byte value to bcd
uint8_t PCF8583Component::byte2bcd_(uint8_t value){
    return ((value / 10) << 4) + (value % 10);
}





}  //namespace esphome
}  //namespace pcf8583
