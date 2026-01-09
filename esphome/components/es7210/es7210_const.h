#pragma once

#include <cinttypes>

namespace esphome {
namespace es7210 {

// ES7210 register addresses
static const uint8_t ES7210_RESET_REG00 = 0x00;     /* Reset control */
static const uint8_t ES7210_CLOCK_OFF_REG01 = 0x01; /* Used to turn off the ADC clock */
static const uint8_t ES7210_MAINCLK_REG02 = 0x02;   /* Set ADC clock frequency division */

static const uint8_t ES7210_MASTER_CLK_REG03 = 0x03; /* MCLK source $ SCLK division */
static const uint8_t ES7210_LRCK_DIVH_REG04 = 0x04;  /* lrck_divh */
static const uint8_t ES7210_LRCK_DIVL_REG05 = 0x05;  /* lrck_divl */
static const uint8_t ES7210_POWER_DOWN_REG06 = 0x06; /* power down */
static const uint8_t ES7210_OSR_REG07 = 0x07;
static const uint8_t ES7210_MODE_CONFIG_REG08 = 0x08;     /* Set primary/secondary & channels */
static const uint8_t ES7210_TIME_CONTROL0_REG09 = 0x09;   /* Set Chip intial state period*/
static const uint8_t ES7210_TIME_CONTROL1_REG0A = 0x0A;   /* Set Power up state period */
static const uint8_t ES7210_SDP_INTERFACE1_REG11 = 0x11;  /* Set sample & fmt */
static const uint8_t ES7210_SDP_INTERFACE2_REG12 = 0x12;  /* Pins state */
static const uint8_t ES7210_ADC_AUTOMUTE_REG13 = 0x13;    /* Set mute */
static const uint8_t ES7210_ADC34_MUTERANGE_REG14 = 0x14; /* Set mute range */
static const uint8_t ES7210_ADC12_MUTERANGE_REG15 = 0x15; /* Set mute range */
static const uint8_t ES7210_ADC34_HPF2_REG20 = 0x20;      /* HPF */
static const uint8_t ES7210_ADC34_HPF1_REG21 = 0x21;      /* HPF */
static const uint8_t ES7210_ADC12_HPF1_REG22 = 0x22;      /* HPF */
static const uint8_t ES7210_ADC12_HPF2_REG23 = 0x23;      /* HPF */
static const uint8_t ES7210_ANALOG_REG40 = 0x40;          /* ANALOG Power */
static const uint8_t ES7210_MIC12_BIAS_REG41 = 0x41;
static const uint8_t ES7210_MIC34_BIAS_REG42 = 0x42;
static const uint8_t ES7210_MIC1_GAIN_REG43 = 0x43;
static const uint8_t ES7210_MIC2_GAIN_REG44 = 0x44;
static const uint8_t ES7210_MIC3_GAIN_REG45 = 0x45;
static const uint8_t ES7210_MIC4_GAIN_REG46 = 0x46;
static const uint8_t ES7210_MIC1_POWER_REG47 = 0x47;
static const uint8_t ES7210_MIC2_POWER_REG48 = 0x48;
static const uint8_t ES7210_MIC3_POWER_REG49 = 0x49;
static const uint8_t ES7210_MIC4_POWER_REG4A = 0x4A;
static const uint8_t ES7210_MIC12_POWER_REG4B = 0x4B; /* MICBias & ADC & PGA Power */
static const uint8_t ES7210_MIC34_POWER_REG4C = 0x4C;

/*
 * Clock coefficient structure
 */
struct ES7210Coefficient {
  uint32_t mclk;  // mclk frequency
  uint32_t lrclk;
  uint8_t ss_ds;
  uint8_t adc_div;
  uint8_t dll;       // dll_bypass
  uint8_t doubler;   // doubler_enable
  uint8_t osr;       // adc osr
  uint8_t mclk_src;  // sselect mclk source
  uint8_t lrck_h;    // High 4 bits of lrck
  uint8_t lrck_l;    // Low 8 bits of lrck
};

/* Codec hifi mclk clock divider coefficients
 *           MEMBER      REG
 *           mclk:       0x03
 *           lrck:       standard
 *           ss_ds:      --
 *           adc_div:    0x02
 *           dll:        0x06
 *           doubler:    0x02
 *           osr:        0x07
 *           mclk_src:   0x03
 *           lrckh:      0x04
 *           lrckl:      0x05
 */
static const ES7210Coefficient ES7210_COEFFICIENTS[] = {
    // mclk      lrck    ss_ds adc_div  dll  doubler osr  mclk_src  lrckh   lrckl
    /* 8k */
    {12288000, 8000, 0x00, 0x03, 0x01, 0x00, 0x20, 0x00, 0x06, 0x00},
    {16384000, 8000, 0x00, 0x04, 0x01, 0x00, 0x20, 0x00, 0x08, 0x00},
    {19200000, 8000, 0x00, 0x1e, 0x00, 0x01, 0x28, 0x00, 0x09, 0x60},
    {4096000, 8000, 0x00, 0x01, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00},

    /* 11.025k */
    {11289600, 11025, 0x00, 0x02, 0x01, 0x00, 0x20, 0x00, 0x01, 0x00},

    /* 12k */
    {12288000, 12000, 0x00, 0x02, 0x01, 0x00, 0x20, 0x00, 0x04, 0x00},
    {19200000, 12000, 0x00, 0x14, 0x00, 0x01, 0x28, 0x00, 0x06, 0x40},

    /* 16k */
    {4096000, 16000, 0x00, 0x01, 0x01, 0x01, 0x20, 0x00, 0x01, 0x00},
    {19200000, 16000, 0x00, 0x0a, 0x00, 0x00, 0x1e, 0x00, 0x04, 0x80},
    {16384000, 16000, 0x00, 0x02, 0x01, 0x00, 0x20, 0x00, 0x04, 0x00},
    {12288000, 16000, 0x00, 0x03, 0x01, 0x01, 0x20, 0x00, 0x03, 0x00},

    /* 22.05k */
    {11289600, 22050, 0x00, 0x01, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00},

    /* 24k */
    {12288000, 24000, 0x00, 0x01, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00},
    {19200000, 24000, 0x00, 0x0a, 0x00, 0x01, 0x28, 0x00, 0x03, 0x20},

    /* 32k */
    {12288000, 32000, 0x00, 0x03, 0x00, 0x00, 0x20, 0x00, 0x01, 0x80},
    {16384000, 32000, 0x00, 0x01, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00},
    {19200000, 32000, 0x00, 0x05, 0x00, 0x00, 0x1e, 0x00, 0x02, 0x58},

    /* 44.1k */
    {11289600, 44100, 0x00, 0x01, 0x01, 0x01, 0x20, 0x00, 0x01, 0x00},

    /* 48k */
    {12288000, 48000, 0x00, 0x01, 0x01, 0x01, 0x20, 0x00, 0x01, 0x00},
    {19200000, 48000, 0x00, 0x05, 0x00, 0x01, 0x28, 0x00, 0x01, 0x90},

    /* 64k */
    {16384000, 64000, 0x01, 0x01, 0x01, 0x00, 0x20, 0x00, 0x01, 0x00},
    {19200000, 64000, 0x00, 0x05, 0x00, 0x01, 0x1e, 0x00, 0x01, 0x2c},

    /* 88.2k */
    {11289600, 88200, 0x01, 0x01, 0x01, 0x01, 0x20, 0x00, 0x00, 0x80},

    /* 96k */
    {12288000, 96000, 0x01, 0x01, 0x01, 0x01, 0x20, 0x00, 0x00, 0x80},
    {19200000, 96000, 0x01, 0x05, 0x00, 0x01, 0x28, 0x00, 0x00, 0xc8},
};

static const float ES7210_MIC_GAIN_MIN = 0.0;
static const float ES7210_MIC_GAIN_MAX = 37.5;

}  // namespace es7210
}  // namespace esphome
