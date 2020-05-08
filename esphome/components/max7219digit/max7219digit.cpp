#include "max7219digit.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "max7219font.h"

namespace esphome {
namespace max7219digit {

static const char *TAG = "max7219DIGIT";

static const uint8_t MAX7219_REGISTER_NOOP = 0x00;
static const uint8_t MAX7219_REGISTER_DECODE_MODE = 0x09;
static const uint8_t MAX7219_REGISTER_INTENSITY = 0x0A;
static const uint8_t MAX7219_REGISTER_SCAN_LIMIT = 0x0B;
static const uint8_t MAX7219_REGISTER_SHUTDOWN = 0x0C;
static const uint8_t MAX7219_REGISTER_DISPLAY_TEST = 0x0F;
constexpr uint8_t MAX7219_NO_SHUTDOWN = 0x00;
constexpr uint8_t MAX7219_SHUTDOWN = 0x01;
constexpr uint8_t MAX7219_NO_DISPLAY_TEST = 0x00;
constexpr uint8_t MAX7219_DISPLAY_TEST = 0x01;


float MAX7219Component::get_setup_priority() const { return setup_priority::PROCESSOR; }

void MAX7219Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MAX7219_DIGITS...");
  this->spi_setup();
  this->stepsleft=0;
  this->init_internal_(this->get_buffer_length_());
  for (uint8_t i = 0; i < this->get_buffer_length_(); i++){  // Clear buffer for startup
    this->buffer_[i] = 0;
  }

  // let's assume the user has all 8 digits connected, only important in daisy chained setups anyway
  this->send_to_all_(MAX7219_REGISTER_SCAN_LIMIT, 7);
  // let's use our own ASCII -> led pattern encoding
  this->send_to_all_(MAX7219_REGISTER_DECODE_MODE, 0);
  // No display test with all the pixels on  
  this->send_to_all_(MAX7219_REGISTER_DISPLAY_TEST, MAX7219_NO_DISPLAY_TEST);
  // SET Intsity of display
  this->send_to_all_(MAX7219_REGISTER_INTENSITY, this->intensity_);
  //this->send_to_all_(MAX7219_REGISTER_INTENSITY, 1);
  this->display();
  // power up
  this->send_to_all_(MAX7219_REGISTER_SHUTDOWN, 1);
  ESP_LOGD(TAG,"MAX7219 DIGIT SETUP STARTED");
}
void MAX7219Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX7219DIGIT:");
  ESP_LOGCONFIG(TAG, "  Number of Chips: %u", this->num_chips_);
  ESP_LOGCONFIG(TAG, "  Intensity: %u", this->intensity_);
  ESP_LOGCONFIG(TAG, "  Offset: %u", this->offset_chips);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

void MAX7219Component::display() {
byte pixels [8];
  for (uint8_t i = 0; i < this->num_chips_; i++) {        //Run this loop for every MAX CHIP (GRID OF 64 leds)
    for (uint8_t j = 0; j < 8; j++) {                     //Run this routine for the rows of every chip 8x row 0 top to 7 bottom
      pixels[j]=this->buffer_[i * 8 + j];                 //Fill the pixel parameter with diplay data
    }                                                      
    this->send64pixels(i,pixels);                         //Send the data to the chip
  }
  ESP_LOGD(TAG,"Display Called");                         //TEMP DEBUG INFO TO BE DELETED
}

int MAX7219Component::get_height_internal(){
  return 8;                                               //TO BE DONE -> STACK TWO DISPLAYS ON TOP OF EACH OTHE
                                                          //TO BE DONE -> CREATE Virtual size of screen and scroll
}

int MAX7219Component::get_width_internal(){
  return (this->num_chips_+this->offset_chips)*8;                              
}

size_t MAX7219Component::get_buffer_length_(){
  return (this->num_chips_+this->offset_chips)*8;                              
}

void HOT MAX7219Component::draw_absolute_pixel_internal(int x, int y, int color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)    //If pixel is outside display then dont draw
    return;
  if (x > this->max_x) this->max_x = x;
  //ESP_LOGD(TAG,"x %i and max x %i",x,this->max_x);                   // Set MAX X to be used in further function
  uint16_t pos = x;                                       // X is starting at 0 top left
  uint8_t subpos = y;                                     // Y is starting at 0 top left
  if (color) {
    if (this->invert){
      this->buffer_[pos] ^= (1 << subpos);
    } else {
      this->buffer_[pos] |= (1 << subpos);
    }
  } else {
    this->buffer_[pos] &= ~(1 << subpos);                 // shift a bit to the correct position within the buffer data
  }
}

void MAX7219Component::send_byte_(uint8_t a_register, uint8_t data) {
  this->write_byte(a_register);                           // Write register value to MAX
  this->write_byte(data);                                 // Followed by actual data
}
void MAX7219Component::send_to_all_(uint8_t a_register, uint8_t data) {
  this->enable();                                         // Enable SPI
  for (uint8_t i = 0; i < this->num_chips_; i++)          // Run the loop for every MAX chip in the stack
    this->send_byte_(a_register, data);                   // Send the data to the chips
  this->disable();                                        // Disable SPI
}
void MAX7219Component::update() {
  ESP_LOGD(TAG,"UPDATE CALLED"); 
  max_x=0;                                                  //Debug feedback for testing update is triggered by polling component
  for (uint8_t i = 0; i < this->get_buffer_length_(); i++)  //run this loop for chips*8 (all display positions)
    if (this->invert) {
      this->buffer_[i] = 0xFF;
    }
    else
    {
      this->buffer_[i] = 0;
    }
                                 //clear buffer on every position
  if (this->writer_.has_value())                      //inser Labda function if available
    (*this->writer_)(*this);                         
  this->display();                                  //call display to write buffer
}

void MAX7219Component::invert_on_off(bool on_off){
  this->invert = on_off;
}

void MAX7219Component::invert_on_off(){
  if (this->invert){
    this->invert = false;
  } else {
    this->invert = true;
  }
}

void MAX7219Component::turn_on_off(bool on_off){
  if (on_off) {
    this->send_to_all_(MAX7219_REGISTER_SHUTDOWN, 1);
  } else {
    this->send_to_all_(MAX7219_REGISTER_SHUTDOWN, 0);
  }
}

void MAX7219Component::scroll_left (uint8_t stepsize){
  uint8_t NumSteps = stepsize + this -> stepsleft;
  //uint8 n = this->get_buffer_length_();
  //if (NumSteps==this->get_buffer_length_()) 
  if (this->max_x < this->num_chips_*8) this->max_x = this->num_chips_*8;
  uint8_t n = this->max_x+3;
  ESP_LOGD(TAG,"n: %i",n);
  if (NumSteps >= this->max_x+3) 
    NumSteps = 0;
  this->stepsleft = NumSteps;
  ESP_LOGD(TAG,"NumSteps: %i",NumSteps);
  for(uint8_t j=1;j<NumSteps+1;j++){
    byte temp = this->buffer_[0]; //remember first element
    for(uint8_t i=0;i<n-1;i++)
    {
      this->buffer_[i] = this->buffer_[i+1]; //move all element to the left except first one
    }
    this->buffer_[n-1] = temp; //assign remembered value to last element
  }
}

void MAX7219Component::sendChar (const byte chip, const byte data)
  {
  // get this character from PROGMEM
  //byte pixels [8];
  //for (byte i = 0; i < this->offset_char; i++)
  //   pixels[i]=0;
  for (byte i = 0; i < 8; i++)
     this->buffer_[chip*8+i] = pgm_read_byte (&MAX7219_Dot_Matrix_font [data] [i]);
     //pixels [i+this->offset_char] = pgm_read_byte (&MAX7219_Dot_Matrix_font [data] [i]);
  //this->send64pixels (chip, pixels);
  }  // end of sendChar

// send one character (data) to position (chip)

void MAX7219Component::send64pixels (const byte chip, const byte pixels [8])
  {
  for (byte col = 0; col < 8; col++)    //RUN THIS LOOP 8 times until column is 7
    {
    this->enable();                     // start sending by enabling SPI 
    for (byte i = 0; i < chip; i++)     // send extra NOPs to push the pixels out to extra displays
      this->send_byte_ (MAX7219_REGISTER_NOOP, MAX7219_REGISTER_NOOP);  //run this loop unit the matching chip is reached
    byte b = 0;                         // rotate pixels 90 degrees -- set byte to 0
    for (byte i = 0; i < 8; i++)        //  run this loop 8 times for all the pixels[8] received
      b |= bitRead (pixels [i], col) << (7 - i);  // change the column bits into row bits
    this->send_byte_(col + 1 , b);       // send this byte to dispay at selected chip
    for (int i = 0; i < this->num_chips_ - chip - 1; i++) // end with enough NOPs so later chips don't update
      this->send_byte_ (MAX7219_REGISTER_NOOP, MAX7219_REGISTER_NOOP);
    this->disable();                    // all done disable SPI
    }   // end of for each column
  }  // end of send64pixels

uint8_t MAX7219Component::printdigit(const char *str) { return this->printdigit(0, str); }

uint8_t MAX7219Component::printdigit(uint8_t start_pos, const char *s)
{
  byte chip;
  for (chip = start_pos; chip < this->num_chips_ && *s; chip++)
      sendChar (chip, *s++);
 // space out rest
  while (chip < (this->num_chips_))
    sendChar (chip++, ' ');
return 0;
}  // end of sendString

uint8_t MAX7219Component::printdigitf(uint8_t pos, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->printdigit(pos, buffer);
  return 0;
}
uint8_t MAX7219Component::printdigitf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->printdigit(buffer);
  return 0;
}
void MAX7219Component::set_writer(max7219_writer_t &&writer) { this->writer_ = writer; }
void MAX7219Component::set_intensity(uint8_t intensity) { this->intensity_ = intensity; }
void MAX7219Component::set_num_chips(uint8_t num_chips) { this->num_chips_ = num_chips; }
void MAX7219Component::set_offset(uint8_t offset) { this->offset_chips = offset; }

#ifdef USE_TIME
uint8_t MAX7219Component::strftimedigit(uint8_t pos, const char *format, time::ESPTime time) {
  char buffer[64];
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0)
    return this->printdigit(pos, buffer);
  return 0;
}
uint8_t MAX7219Component::strftimedigit(const char *format, time::ESPTime time) { return this->strftimedigit(0, format, time); }
#endif

}  // namespace max7219
}  // namespace esphome
