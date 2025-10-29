#pragma once
#include <cstdint>

namespace esphome {
namespace es8388 {

/* ES8388 register */
static const uint8_t ES8388_CONTROL1 = 0x00;
static const uint8_t ES8388_CONTROL2 = 0x01;

static const uint8_t ES8388_CHIPPOWER = 0x02;

static const uint8_t ES8388_ADCPOWER = 0x03;
static const uint8_t ES8388_DACPOWER = 0x04;

static const uint8_t ES8388_CHIPLOPOW1 = 0x05;
static const uint8_t ES8388_CHIPLOPOW2 = 0x06;

static const uint8_t ES8388_ANAVOLMANAG = 0x07;

static const uint8_t ES8388_MASTERMODE = 0x08;

/* ADC */
static const uint8_t ES8388_ADCCONTROL1 = 0x09;
static const uint8_t ES8388_ADCCONTROL2 = 0x0a;
static const uint8_t ES8388_ADCCONTROL3 = 0x0b;
static const uint8_t ES8388_ADCCONTROL4 = 0x0c;
static const uint8_t ES8388_ADCCONTROL5 = 0x0d;
static const uint8_t ES8388_ADCCONTROL6 = 0x0e;
static const uint8_t ES8388_ADCCONTROL7 = 0x0f;
static const uint8_t ES8388_ADCCONTROL8 = 0x10;
static const uint8_t ES8388_ADCCONTROL9 = 0x11;
static const uint8_t ES8388_ADCCONTROL10 = 0x12;
static const uint8_t ES8388_ADCCONTROL11 = 0x13;
static const uint8_t ES8388_ADCCONTROL12 = 0x14;
static const uint8_t ES8388_ADCCONTROL13 = 0x15;
static const uint8_t ES8388_ADCCONTROL14 = 0x16;
/* DAC */
static const uint8_t ES8388_DACCONTROL1 = 0x17;
static const uint8_t ES8388_DACCONTROL2 = 0x18;
static const uint8_t ES8388_DACCONTROL3 = 0x19;
static const uint8_t ES8388_DACCONTROL4 = 0x1a;
static const uint8_t ES8388_DACCONTROL5 = 0x1b;
static const uint8_t ES8388_DACCONTROL6 = 0x1c;
static const uint8_t ES8388_DACCONTROL7 = 0x1d;
static const uint8_t ES8388_DACCONTROL8 = 0x1e;
static const uint8_t ES8388_DACCONTROL9 = 0x1f;
static const uint8_t ES8388_DACCONTROL10 = 0x20;
static const uint8_t ES8388_DACCONTROL11 = 0x21;
static const uint8_t ES8388_DACCONTROL12 = 0x22;
static const uint8_t ES8388_DACCONTROL13 = 0x23;
static const uint8_t ES8388_DACCONTROL14 = 0x24;
static const uint8_t ES8388_DACCONTROL15 = 0x25;
static const uint8_t ES8388_DACCONTROL16 = 0x26;
static const uint8_t ES8388_DACCONTROL17 = 0x27;
static const uint8_t ES8388_DACCONTROL18 = 0x28;
static const uint8_t ES8388_DACCONTROL19 = 0x29;
static const uint8_t ES8388_DACCONTROL20 = 0x2a;
static const uint8_t ES8388_DACCONTROL21 = 0x2b;
static const uint8_t ES8388_DACCONTROL22 = 0x2c;
static const uint8_t ES8388_DACCONTROL23 = 0x2d;
static const uint8_t ES8388_DACCONTROL24 = 0x2e;
static const uint8_t ES8388_DACCONTROL25 = 0x2f;
static const uint8_t ES8388_DACCONTROL26 = 0x30;
static const uint8_t ES8388_DACCONTROL27 = 0x31;
static const uint8_t ES8388_DACCONTROL28 = 0x32;
static const uint8_t ES8388_DACCONTROL29 = 0x33;
static const uint8_t ES8388_DACCONTROL30 = 0x34;

static const uint8_t ES8388_DAC_OUTPUT_NONE = 0xC0;  // ALL DAC DOWN

static const uint8_t ES8388_DAC_OUTPUT_LOUT1_ROUT1 = 0x30;
static const uint8_t ES8388_DAC_OUTPUT_LOUT2_ROUT2 = 0x0C;
static const uint8_t ES8388_DAC_OUTPUT_BOTH = 0x3C;

static const uint8_t ES8388_ADC_INPUT_LINPUT1_RINPUT1 = 0x00;
static const uint8_t ES8388_ADC_INPUT_MIC1 = 0x05;
static const uint8_t ES8388_ADC_INPUT_MIC2 = 0x06;
static const uint8_t ES8388_ADC_INPUT_LINPUT2_RINPUT2 = 0x50;
static const uint8_t ES8388_ADC_INPUT_DIFFERENCE = 0xf0;

}  // namespace es8388
}  // namespace esphome
