#include "mcp3911.h"
#include "esphome/core/log.h"
#include "esphome/components/ota/ota_component.h"

namespace esphome {
namespace mcp3911 {

static const char *const TAG = "mcp3911";

  void MCP3911InteruptStore::gpio_intr(MCP3911InteruptStore *store) { store->dataready = true; }
  /*
  void IRAM_ATTR MCP3911::dataready_isr(MCP3911 *arg) {
      //double val=0;
      //arg->c.status.read_reg_incr  = TYPE;
      //arg->reg_write(REG_STATUSCOM, REGISTER, 2);
      arg->data_ready = false;
      arg->reg_read(REG_CHANNEL_0, TYPE);
      arg->data_ready = true;
  }*/

  void MCP3911::loop() {
	if (!this->store_.dataready && this->dr_pin_ != nullptr)
  		return;
        //arg->c.status.read_reg_incr  = TYPE;
        //arg->reg_write(REG_STATUSCOM, REGISTER, 2);
        this->reg_read(REG_CHANNEL_0, TYPE);
        this->store_.dataready = false;

        //is this required? For high frequency loop probably
        delayMicroseconds(2);
  }

  void MCP3911::setup() {
    ESP_LOGCONFIG(TAG, "Setting up MCP3911...");
    Vref = 1.2023f;

    ota::global_ota_component->add_on_state_callback([this](ota::OTAState state, float progress, uint8_t error) {
      if (state == ota::OTA_STARTED) {
        this->stop_mcp3911();
         this->high_freq_.stop();
      }
    });

    this->high_freq_.start();

    /*
    //Allow watchdog timer to timeout at 1000ms to allow ISr
    rtc_wdt_set_length_of_reset_signal(RTC_WDT_SYS_RESET_SIG, RTC_WDT_LENGTH_3_2us);
    rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_RESET_SYSTEM);
    rtc_wdt_set_time(RTC_WDT_STAGE0, 5000);
    */
    
    //Setup outputs (4MHz clock and reset from mcu)
    this->rst_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->rst_pin_->setup();
    this->rst_pin_->digital_write(true);

    this->clk_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->clk_pin_->setup();

    //Start 4MHz clock 
    ledc_channel.gpio_num = (int16_t) this->clk_pin_->get_pin();
    ledc_timer_config(&ledc_timer);
    ledc_channel_config(&ledc_channel);
         
    //Bind interupt to dataready pin (ISR)
    if (this->dr_pin_ != nullptr) {
      this->dr_pin_->pin_mode(gpio::FLAG_INPUT);
      this->dr_pin_->setup();
      this->store_.pin = this->dr_pin_->to_isr();
      this->dr_pin_->attach_interrupt(MCP3911InteruptStore::gpio_intr, &this->store_, gpio::INTERRUPT_FALLING_EDGE); 
    }
    
    this->spi_setup();
    this->reset_mcp3911();
    
    //Start with reading the register 
    reg_read(REG_STATUSCOM, REGISTER, 2);

    //this->exit_reset_mode();

    //set reading and writing of all 27 registers
    c.status.read_reg_incr  = ALL;
    c.status.write_reg_incr = ALL;
    reg_write(REG_STATUSCOM, REGISTER, 2);
    
    // read all registers from beginning (defaults)
    reg_read(REG_CHANNEL_0, ALL);

    c.gain.ch0              =  gain_; //GAINX1;
    c.gain.boost            =  boost_; //BOOSTX2;
    
    c.gain.ch1              =  gain_; //GAINX1;
    c.gain.boost            =  boost_; //BOOSTX2;

    
    // modulator out on pins MDAT1/MDAT2
    c.status.ch0_modout     = 0;
    c.status.ch1_modout     = 0;

    // data ready pin is logic high = 1 when data not ready, data ready pin is high-impeadance when data not ready  = 0
    c.status.dr_pull_up     = 1;
    // signal MCU when both ADC registers with values are ready
    c.status.dr_mode = CH_BOTH;

    // auto zeroing running on high speed or on low speed
    c.config.az_on_hi_speed = 1;

    // internal clock selection - when 0 a crystal should be placed between OSC1 and OSC2
    c.config.clkext         = 1;
    
    // Internal Voltage Reference Shutdown Control - 0 is enabled (default)
    c.config.vrefext        = 0;

    // oversampling Ratio for Delta-Sigma A/D Conversion
    c.config.osr            = oversampling_; //O4096;
    c.config.prescale       = prescale_; //MCLK4;
    
    // control for dithering circuit for idle tones cancellation and improved THD
    c.config.dither         = dither_; //MAX;

    c.config.ch0_shtdn_mode = 0;
    c.config.ch1_shtdn_mode = 0;
    c.config.reset_ch0 = 0;
    c.config.reset_ch1 = 0;

    
    //Read all 27 registers, write 
    c.status.read_reg_incr = ALL;
    c.status.write_reg_incr = TYPE;

    // vref temperature compensator adjustment
    c.vrefcal += 0x11;
    //c.vrefcal = 0x42; // default value

    // write out all regs
    reg_write(REG_MOD, TYPE);

    // set grouping for GROUP, and so prepare the chip to read 3 value registers for channel  in the sample method of the sensor
    c.status.read_reg_incr  = TYPE;
    reg_write(REG_STATUSCOM, REGISTER, 2);
  }

  void MCP3911::dump_config() {
    ESP_LOGCONFIG(TAG, "MCP3911 Config:"); 
    LOG_PIN("  CS Pin:", this->cs_);
    LOG_PIN("  4MHz CLK Pin:", this->clk_pin_);
    LOG_PIN("  Reset Pin: ", this->rst_pin_);

    ESP_LOGCONFIG(TAG, "Boost: %d", boost_);
    ESP_LOGCONFIG(TAG, "Gain: %d", gain_);
    ESP_LOGCONFIG(TAG, "Prescale: %d", prescale_);
    ESP_LOGCONFIG(TAG, "Dither: %d", dither_);
    ESP_LOGCONFIG(TAG, "Oversampling: %d", oversampling_);
    //setup();
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
      this->enable();
      this->transfer_byte(addr << 1 | 1);
      uint8_t buff;
      uint16_t curaddr = addr;
      while (count--) {
          buff = this->transfer_byte(0xff);
          *buffer++ = buff;
          //ESP_LOGCONFIG(TAG, "Read SPI from addr: %d - byte: %d, %02x", curaddr++, buff, buff);
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
      uint16_t curaddr = addr;
      this->enable();
      this->transfer_byte(addr << 1 | 0);
      while (count--) {
          this->transfer_byte(*buffer++);
	  //ESP_LOGCONFIG(TAG, "Wrote SPI to addr: %d - bytes: %d, %02x", curaddr, *buffer, *buffer);
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

  void MCP3911::exit_reset_mode(void) {
      const uint8_t r = 0b00111111;
      SPI_write(REG_CONFIG2, (uint8_t *)&r, 1);
  }

  void MCP3911::enter_reset_mode(void) {
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
                            case REG_CHANNEL_0: count = 27; break;
                        }
                        break;
              default: return false;
          }
      }
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

  void MCP3911::reset_mcp3911() {
     this->rst_pin_->digital_write(false);
     delayMicroseconds(100);
     this->rst_pin_->digital_write(true);
  }

  void MCP3911::stop_mcp3911() {
     this->rst_pin_->digital_write(false);
  }

  void MCP3911::start_mcp3911() {
     this->rst_pin_->digital_write(true);
  }


}
}
