#include "mcp3911.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp3911 {

static const char *const TAG = "mcp3911";

  void MCP3911::setup() {
    ESP_LOGCONFIG(TAG, "Setting up MCP3911...");

    Vref = 1.2023f;
    
    this->spi_setup();
    this->reset();
     
    reg_read(REG_STATUSCOM, REGISTER, 2);
    c.status.read_reg_incr  = ALL;
    c.status.write_reg_incr = ALL;
    reg_write(REG_STATUSCOM, REGISTER, 2);
    
    // read default values
    reg_read(REG_CHANNEL_0, ALL);
/*
    c.gain.ch0              =  gain_; //GAINX1;
    c.gain.boost            =  boost_; //BOOSTX2;
    
    c.gain.ch1              =  gain_; //GAINX1;
    c.gain.boost            =  boost_; //BOOSTX2;

    
    // modulator out on pins MDAT1/MDAT2
    c.status.ch0_modout     = 0;

    // data ready pin is logic high = 1 when data not ready, data ready pin is high-impeadance when data not ready  = 0
    c.status.dr_pull_up     = 1;

    // auto zeroing running on high speed or on low speed
    c.config.az_on_hi_speed = 0;

    // internal clock selection - when 0 a crystal should be placed between OSC1 and OSC2
    c.config.clkext         = 1;
    
    // Internal Voltage Reference Shutdown Control - 0 is enabled (default)
    c.config.vrefext        = 0;

    // oversampling Ratio for Delta-Sigma A/D Conversion
    c.config.osr            = oversampling_; //O4096;
    c.config.prescale       = prescale_; //MCLK4;

    // control for dithering circuit for idle tones cancellation and improved THD
    c.config.dither         = dither_; //MAX;

    c.status.read_reg_incr = ALL;
    c.status.write_reg_incr = TYPE;

    // vref temperature compensator adjustment
    c.vrefcal += 0x11;
    //c.vrefcal = 0x42; // default value
*/ 
    // write out all regs
    reg_write(REG_MOD, TYPE);

    // set grouping for TYPE
    c.status.read_reg_incr  = TYPE;
    reg_write(REG_STATUSCOM, REGISTER, 2);
  }

  void MCP3911::dump_config() {
    ESP_LOGCONFIG(TAG, "MCP3911 Config:"); 
    ESP_LOGCONFIG(TAG, "Boost: %d", boost_);
    ESP_LOGCONFIG(TAG, "Gain: %d", gain_);
    ESP_LOGCONFIG(TAG, "Prescale: %d", prescale_);
    ESP_LOGCONFIG(TAG, "Dither: %d", dither_);
    ESP_LOGCONFIG(TAG, "Oversampling: %d", oversampling_);
    setup();
    LOG_PIN("  CS Pin:", this->cs_);
  }

  /*
  void MCP3911::update() {
    double v0,v1;
    
    this->reg_read(REG_CHANNEL_0, TYPE); // read 6 regs  
    this->get_value(&v0, 0);
    this->get_value(&v1, 1);

    ESP_LOGD(TAG, "CH0 Voltage %d", v0);
    ESP_LOGD(TAG, "CH1 Voltage %d", v1);

    this->ch0->publish_state(v0);
    this->ch1->publish_state(v1);

    publish_state(0);
  }
  */

   void MCP3911::SPI_read(uint8_t addr, uint8_t *buffer, size_t count) {
      ESP_LOGCONFIG(TAG, "Reading SPI bytes: %d", count);
      this->enable();
      this->transfer_byte(addr << 1 | 1);
      uint8_t buff;
      while (count--) {
          buff = this->transfer_byte(0xff);
          *buffer++ = buff;
          ESP_LOGCONFIG(TAG, "Read SPI from addr: %d - byte: %d, %02x", addr, buff, buff);
      }
      this->disable();

      /*
      digitalWrite(ADC_CS_PIN, LOW);
      SPI.transfer(addr << 1 | 1);
      while (count--)
        *buffer++ = SPI.transfer(0xff);
      digitalWrite(ADC_CS_PIN, HIGH);
      */
  }

  void MCP3911::SPI_write(uint8_t addr, uint8_t *buffer, size_t count) {
      ESP_LOGCONFIG(TAG, "Writting SPI bytes: %d", count);
      this->enable();
      this->transfer_byte(addr << 1 | 0);
      while (count--) {
          this->transfer_byte(*buffer++);
	  ESP_LOGCONFIG(TAG, "Wrote SPI to addr: %d - bytes: %d, %02x", addr, *buffer, *buffer);
      }
      this->disable();
      /*
      digitalWrite(ADC_CS_PIN, LOW);
      SPI.transfer(addr << 1 | 0);
      while (count--)
        SPI.transfer(*buffer++);
      digitalWrite(ADC_CS_PIN, HIGH);
      */
  }

  void MCP3911::reset(void) {
      const uint8_t r = 0b11000000;
      SPI_write(REG_CONFIG2, (uint8_t *)&r, 1); // 6.7.2 p. 47
  }

  bool MCP3911::reg_read(uint8_t addr, Looping g, uint8_t count) {
      uint8_t *data = ((uint8_t *)&c)+addr;
      if (count == 0) {
          switch (g) {
              case REGISTER: count = 1; 
                             break;
              case GROUP: switch (addr) {
                              case REG_CHANNEL_0: 
                              case REG_CHANNEL_1:  count = 3; break;
                              case REG_MOD: 
                              case REG_STATUSCOM:  count = 4; break;
                              case REG_OFFCAL_CH0:
                              case REG_OFFCAL_CH1: count = 6; break;
                              case REG_VREFCAL:    count = 1; break;
                          }
                          break;
              case TYPE: switch (addr) {
                             case REG_CHANNEL_0: count = 6; break;
                             case REG_MOD: count = 21; break;
                         }
                         break;
              case ALL: switch (addr) {
                            case REG_CHANNEL_0: count = 0x1b; break;
                        }
                        break;
              default: return false;
          }
      }

      ESP_LOGCONFIG(TAG, "Reading registers: start: %d, size: %d, pointer: %d", addr, count, data);
      SPI_read(addr, data, count);

      return true;
  }

  bool MCP3911::reg_write(uint8_t addr, Looping g, uint8_t count) {
      uint8_t *data = ((uint8_t *)&c)+addr;

      if (count == 0) {
          switch (g) {
              case REGISTER: count = 1; 
                             break;
              case TYPE: switch (addr) {
                             case REG_MOD: count = 21; break;
                         }
                         break;
              case GROUP: break;
              case ALL: break;
              default: return false;
          }}

      SPI_write(addr, data, count);

      return true;
  }

  void MCP3911::get_value(double *result, uint8_t channel) {
      _ChVal& val = c.ch[channel];

      int32_t data_ch = msb2l((uint8_t *)&val, 3);

      if (data_ch & 0x00800000L)
          data_ch |= 0xff000000L;

      uint8_t gain = (channel == 0)? c.gain.ch0 : c.gain.ch1;

      *result = Vref * data_ch / ( (3*32768*256/2) * (1 << gain) );
  }

  // msb array to ulong
  uint32_t MCP3911::msb2l(void *src, uint8_t count) {
      uint32_t value = 0L;
      uint8_t *b = (uint8_t *)src;

      while (count--) {
          value <<= 8;
          value |= *b++;
      }
      return value;
  }

  // ulong to msb array
  void MCP3911::l2msb(uint32_t value, void *tgt, uint8_t count) {
      uint32_t v = value;
      uint8_t *b = (uint8_t *)tgt;

      while (1) {
          count--;
          *(b+count) = (uint8_t)(v & 0xff);
          if (count == 0)
              break;
          v >>= 8;
      }
  }

}
}
