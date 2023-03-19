#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
/*#include "esphome/components/voltage_sampler/voltage_sampler.h"*/

namespace esphome {
namespace mcp3911 {

/*
    #ifndef MCLK
        #warning "MCLK not defined using 4000000"
        #define MCLK 4000000
    #endif
    #define  AMCLK (MCLK >> (*_c).config.prescale)
    #define  DMCLK (AMCLK >> 2)
    #define  DRCLK (DMCLK >> ( 32 << (*_c).config.osr))
*/

    typedef enum {  REG_CHANNEL_0   = 0x00, //24 bit MSB first
                    REG_CHANNEL_1   = 0x03, //24 bit MSB first
                    REG_MOD         = 0x06, //Modulator output register
                    REG_PHASE       = 0x07, //Phase delay configuration register
                    REG_GAIN        = 0x09, //Gain and boost configuration register
                    REG_STATUSCOM   = 0x0a, //Status and communicaton register
                    REG_CONFIG      = 0x0c, //Configuration register
                    REG_CONFIG2     = 0x0c, //Configuration register
                    REG_OFFCAL_CH0  = 0x0e, //Offset correction register CH0
                    REG_GAINCAL_CH0 = 0x11, //Gain correction register CH0
                    REG_OFFCAL_CH1  = 0x14, //Offset correction register CH1
                    REG_GAINCAL_CH1 = 0x17, //Gain correction register CH1
                    REG_VREFCAL     = 0x1a } //Internal Voltage Reference Temperature Coefficient Adjustment Register
                _Regs;

    typedef enum { GAINX1, GAINX2, GAINX4, GAINX8, GAINX16, GAINX32 } Gain;
    typedef enum { BOOSTX05, BOOSTX066, BOOSTX1, BOOSTX2 } Boost;
    typedef enum { O32, O64, O128, O256, O512, O1024, O2048, O4096 } Oversampling;
    typedef enum { MCLK1, MCLK2, MCLK4, MCLK8 } Prescale;
    typedef enum { REGISTER, GROUP, TYPE, ALL } Looping;
    typedef enum { LAGGING, CH0, CH1, CH_BOTH } DR_Mode;
    typedef enum { OFF, MIN, MED, MAX } Dither;

    typedef enum { A, B } _Channel;

    // 3byte data values - msb first
    typedef struct { uint8_t b[3]; } _ChVal;

    // _ConfMod (0x06) 
    typedef struct __attribute__ ((__packed__)) {
        union {
            uint8_t reg;
            struct __attribute__ ((__packed__)) {
                uint8_t    comp_ch0 :4;
                uint8_t    comp_ch1 :4; // msb
            };
        };
    } _ConfMod;

    // _ConfPhase (0x07,0x08)
    typedef struct __attribute__ ((__packed__)) {
        uint8_t b[2];
    } _ConfPhase;

    // _ConfGain (0x09)
    typedef struct __attribute__ ((__packed__)) {
        Gain            ch0:3;
        Gain            ch1:3; 
        Boost         boost:2; // msb
    } _ConfGain;

    // _ConfStatus (0x0a, 0x0b)
    typedef struct __attribute__ ((__packed__)) {
        bool       ch0_not_ready: 1;
        bool       ch1_not_ready: 1;
        DR_Mode          dr_mode: 2;
        bool          dr_pull_up: 1;
        bool                    : 1;
        bool          ch0_modout: 1; // p. 33
        bool          ch1_modout: 1; // msb

        bool                    : 1;
        bool      gain_cal_24bit: 1;
        bool       off_cal_24bit: 1;
        bool      ch0_24bit_mode: 1;
        bool      ch1_24bit_mode: 1;
        bool      write_reg_incr: 1;
        Looping    read_reg_incr: 2; // msb
    } _ConfStatus;

    // _Config (0x0c, 0x0d)
    typedef struct __attribute__ ((__packed__)) {
        bool   az_on_hi_speed: 1; // p. 33
        Dither         dither: 2;
        Oversampling      osr: 3; 
        Prescale     prescale: 2; // msb 15

        bool                 : 1;
        bool           clkext: 1;
        bool          vrefext: 1;
        bool                 : 1;
        bool   ch0_shtdn_mode: 1;
        bool   ch1_shtdn_mode: 1; 
        bool        reset_ch0: 1; 
        bool        reset_ch1: 1; // msb 7
    } _Config;

    // _ConfOffcal (0x0e, 0x14)
    typedef struct __attribute__ ((__packed__)) {
        uint8_t b[3];
    } _ConfOffCal;

    // _ConfGaincal (0x011, 0x17)
    typedef struct __attribute__ ((__packed__)) {
        uint8_t b[3];
    } _ConfGainCal;

    // _ConfRegMap (27) (0x1b)
    typedef struct __attribute__ ((__packed__)) {
        _ChVal              ch[2]; // 0x00 3,3
        _ConfMod              mod; // 0x06 1 
        _ConfPhase          phase; // 0x07 2
        _ConfGain            gain; // 0x09 1
        _ConfStatus        status; // 0x0a 2
        _Config            config; // 0x0c 2
        _ConfOffCal    offcal_ch0; // 0x0e 3
        _ConfGainCal  gaincal_ch0; // 0x11 3
        _ConfOffCal    offcal_ch1; // 0x14 3
        _ConfGainCal  gaincal_ch1; // 0x17 3
        uint8_t           vrefcal; // 0x1a 1
    } _ConfRegMap;

    class MCP3911 : public Component,
		    public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                         spi::DATA_RATE_20MHZ> {
      public:
        MCP3911() = default;

        void setup() override;
        void dump_config() override;
        float get_setup_priority() const { return setup_priority::LATE; }
        //void update();
        bool reg_read(uint8_t addr, Looping g, uint8_t count=0);
        void get_value(double *, uint8_t channel);

        void set_gain(Gain gain) {gain_ = gain;};
        void set_boost(Boost boost) {boost_ = boost;};
        void set_oversampling(Oversampling oversampling) {oversampling_ = oversampling;};
        void set_dither(Dither dither) {dither_ = dither;};
	void set_prescale(Prescale prescale) {prescale_ = prescale;};

      private:
        _ConfRegMap c;
        double Vref = 1.20f;

        Gain gain_;
    	Boost boost_;
    	Oversampling oversampling_;
    	Prescale prescale_;
    	Dither dither_;


        uint16_t read_data(uint8_t channel);
        void SPI_read(uint8_t addr, uint8_t *buffer, size_t count);
        void SPI_write(uint8_t addr, uint8_t *buffer, size_t count);

        void reset(void);

        bool reg_write(uint8_t addr, Looping g, uint8_t count=0);


        uint32_t msb2l(void *src, uint8_t count);
        void l2msb(uint32_t value, void *tgt, uint8_t count);
    };

}
}
