#include "st7789v.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7789v {

static const char *TAG = "st7789v";

void ST7789V::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SPI ST7789V...");
  this->spi_setup();
  this->dc_pin_->setup();  // OUTPUT

  this->init_reset_();
  
  this->writecommand(ST7789_SLPOUT);   // Sleep out
  delay(120);

  this->writecommand(ST7789_NORON);    // Normal display mode on

  //------------------------------display and color format setting--------------------------------//
  this->writecommand(ST7789_MADCTL);
  //writedata(0x00);
  this->writedata(TFT_MAD_COLOR_ORDER);

  // JLX240 display datasheet
  this->writecommand(0xB6);
  this->writedata(0x0A);
  this->writedata(0x82);

  this->writecommand(ST7789_COLMOD);
  this->writedata(0x55);
  delay(10);

  //--------------------------------ST7789V Frame rate setting----------------------------------//
  this->writecommand(ST7789_PORCTRL);
  this->writedata(0x0c);
  this->writedata(0x0c);
  this->writedata(0x00);
  this->writedata(0x33);
  this->writedata(0x33);

  this->writecommand(ST7789_GCTRL);      // Voltages: VGH / VGL
  this->writedata(0x35);

  //---------------------------------ST7789V Power setting--------------------------------------//
  this->writecommand(ST7789_VCOMS);
  this->writedata(0x28);		// JLX240 display datasheet

  this->writecommand(ST7789_LCMCTRL);
  this->writedata(0x0C);

  this->writecommand(ST7789_VDVVRHEN);
  this->writedata(0x01);
  this->writedata(0xFF);

  this->writecommand(ST7789_VRHS);       // voltage VRHS
  this->writedata(0x10);

  this->writecommand(ST7789_VDVSET);
  this->writedata(0x20);

  this->writecommand(ST7789_FRCTR2);
  this->writedata(0x0f);

  this->writecommand(ST7789_PWCTRL1);
  this->writedata(0xa4);
  this->writedata(0xa1);

  //--------------------------------ST7789V gamma setting---------------------------------------//
  this->writecommand(ST7789_PVGAMCTRL);
  this->writedata(0xd0);
  this->writedata(0x00);
  this->writedata(0x02);
  this->writedata(0x07);
  this->writedata(0x0a);
  this->writedata(0x28);
  this->writedata(0x32);
  this->writedata(0x44);
  this->writedata(0x42);
  this->writedata(0x06);
  this->writedata(0x0e);
  this->writedata(0x12);
  this->writedata(0x14);
  this->writedata(0x17);

  this->writecommand(ST7789_NVGAMCTRL);
  this->writedata(0xd0);
  this->writedata(0x00);
  this->writedata(0x02);
  this->writedata(0x07);
  this->writedata(0x0a);
  this->writedata(0x28);
  this->writedata(0x31);
  this->writedata(0x54);
  this->writedata(0x47);
  this->writedata(0x0e);
  this->writedata(0x1c);
  this->writedata(0x17);
  this->writedata(0x1b);
  this->writedata(0x1e);

  this->writecommand(ST7789_INVON);

  this->writecommand(ST7789_CASET);    // Column address set
  this->writedata(0x00);
  this->writedata(0x00);
  this->writedata(0x00);
  this->writedata(0xE5);    // 239

  this->writecommand(ST7789_RASET);    // Row address set
  this->writedata(0x00);
  this->writedata(0x00);
  this->writedata(0x01);
  this->writedata(0x3F);    // 319

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //this->disable();
  delay(120);
  //this->enable();

  this->writecommand(ST7789_DISPON);    //Display on
  delay(120);
  
  backlight(true);
  
  this->init_internal_(this->get_buffer_length_());
  memset(this->buffer_, 0x00, this->get_buffer_length_());
  
  //this->lcdDrawFillRect(1,1,186,279, 0xF800);
  //this->lcdDrawFillRect(1,1,186,279, 0x0000);
  //delay(10);
  //this->lcdDrawFillRect(0,39,186,40, 0x0FFF);
  //this->lcdDrawFillRect(0,39,52,279, 0x0FF0);
}

void ST7789V::write_display_data() {
	uint16_t x1 = 52;// +dev->_offsetx;
	uint16_t x2 = 186;// + dev->_offsetx;
	uint16_t y1 = 40;// + dev->_offsety;
	uint16_t y2 = 279;// + dev->_offsety;
	
	this->enable();

	// set column(x) address
	this->dc_pin_->digital_write(false);
	this->write_byte(0x2A);
	this->dc_pin_->digital_write(true);
	this->spi_master_write_addr(x1, x2);
	
	// set Page(y) address
	this->dc_pin_->digital_write(false);
	this->write_byte(0x2B);
	this->dc_pin_->digital_write(true);
	this->spi_master_write_addr(y1, y2);

	//  Memory Write
	this->dc_pin_->digital_write(false);
	this->write_byte(0x2C);
	this->dc_pin_->digital_write(true);

	this->write_array(this->buffer_, this->get_buffer_length_());

	this->disable();
}

// Draw rectangle of filling
// x1:Start X coordinate
// y1:Start Y coordinate
// x2:End X coordinate
// y2:End Y coordinate
// color:color
void ST7789V::lcdDrawFillRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
	//if (x1 >= dev->_width) return;
	//if (x2 >= dev->_width) x2=dev->_width-1;
	//if (y1 >= dev->_height) return;
	//if (y2 >= dev->_height) y2=dev->_height-1;

	//ESP_LOGD(TAG,"offset(x)=%d offset(y)=%d",dev->_offsetx,dev->_offsety);
	uint16_t _x1 = x1;// +dev->_offsetx;
	uint16_t _x2 = x2;// + dev->_offsetx;
	uint16_t _y1 = y1;// + dev->_offsety;
	uint16_t _y2 = y2;// + dev->_offsety;

	this->enable();
	//this->writecommand(0x2A);	// set column(x) address
	this->dc_pin_->digital_write(false);
	this->write_byte(0x2A);
	this->dc_pin_->digital_write(true);
	//spi_master_write_data_word(dev, _x1);
	//spi_master_write_data_word(dev, _x2);
	this->spi_master_write_addr(_x1, _x2);
	
	//this->writecommand(0x2B);	// set Page(y) address
	this->dc_pin_->digital_write(false);
	this->write_byte(0x2B);
	this->dc_pin_->digital_write(true);
	//spi_master_write_data_word(dev, _y1);
	//spi_master_write_data_word(dev, _y2);
	this->spi_master_write_addr(_y1, _y2);
	//this->writecommand(0x2C);	//  Memory Write
	this->dc_pin_->digital_write(false);
	this->write_byte(0x2C);
	this->dc_pin_->digital_write(true);
	for(int i=_x1;i<=_x2;i++){
		uint16_t size = _y2-_y1+1;
		this->spi_master_write_color(color, size);
	}
	this->disable();
}

void ST7789V::spi_master_write_addr(uint16_t addr1, uint16_t addr2)
{
	static uint8_t Byte[4];
	Byte[0] = (addr1 >> 8) & 0xFF;
	Byte[1] = addr1 & 0xFF;
	Byte[2] = (addr2 >> 8) & 0xFF;
	Byte[3] = addr2 & 0xFF;

	
	this->dc_pin_->digital_write(true);
	this->write_array(Byte, 4);
	
}

void ST7789V::spi_master_write_color(uint16_t color, uint16_t size)
{
	static uint8_t Byte[1024];
	int index = 0;
	for(int i=0;i<size;i++) {
		Byte[index++] = (color >> 8) & 0xFF;
		Byte[index++] = color & 0xFF;
	}

	this->dc_pin_->digital_write(true);
	return write_array(Byte, size*2);

}


void ST7789V::dump_config() {
  LOG_DISPLAY("", "SPI ST7789V", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  B/L Pin: ", this->bl_pin_);
  LOG_UPDATE_INTERVAL(this);
}

float ST7789V::get_setup_priority() const { 
	return setup_priority::PROCESSOR; 
}

void ST7789V::update() {
  this->do_update_();
  this->write_display_data();
}

void ST7789V::loop() {

}

int ST7789V::get_width_internal() {
	return 135;//240;
}

int ST7789V::get_height_internal() {
	return 240;//320;
}

size_t ST7789V::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) * 2;
}

void HOT ST7789V::draw_absolute_pixel_internal(int x, int y, int color) {
	if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
		return;

	uint16_t pos = (x + y * this->get_width_internal())*2;
	this->buffer_[pos++] |= (color>>8) & 0xff;
	this->buffer_[pos] |= color & 0xff;
}

void ST7789V::init_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(1);
    // Trigger Reset
    this->reset_pin_->digital_write(false);
    delay(10);
    // Wake up
    this->reset_pin_->digital_write(true);
  }
}

void ST7789V::backlight(bool onoff){
  if (this->bl_pin_ != nullptr) {
    this->bl_pin_->setup();

    this->bl_pin_->digital_write(onoff);
  }
}

void ST7789V::writecommand(uint8_t value) {
  this->enable();
  this->dc_pin_->digital_write(false);
  this->write_byte(value);
  this->dc_pin_->digital_write(true);
  this->disable();
}

void ST7789V::writedata(uint8_t value) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_byte(value);
  this->disable();
}

}  // namespace st7789v
}  // namespace esphome
