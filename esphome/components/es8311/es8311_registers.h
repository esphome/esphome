#pragma once

namespace esphome {
namespace es8311 {

static const uint8_t ES8311_REG00_RESET = 0x00;        // Reset
static const uint8_t ES8311_REG01_CLK_MANAGER = 0x01;  // Clock Manager: select clk src for mclk, enable clock for codec
static const uint8_t ES8311_REG02_CLK_MANAGER = 0x02;  // Clock Manager: clk divider and clk multiplier
static const uint8_t ES8311_REG03_CLK_MANAGER = 0x03;  // Clock Manager: adc fsmode and osr
static const uint8_t ES8311_REG04_CLK_MANAGER = 0x04;  // Clock Manager: dac osr
static const uint8_t ES8311_REG05_CLK_MANAGER = 0x05;  // Clock Manager: clk divider for adc and dac
static const uint8_t ES8311_REG06_CLK_MANAGER = 0x06;  // Clock Manager: bclk inverter BIT(5) and divider
static const uint8_t ES8311_REG07_CLK_MANAGER = 0x07;  // Clock Manager: tri-state, lrck divider
static const uint8_t ES8311_REG08_CLK_MANAGER = 0x08;  // Clock Manager: lrck divider
static const uint8_t ES8311_REG09_SDPIN = 0x09;        // Serial Digital Port: DAC
static const uint8_t ES8311_REG0A_SDPOUT = 0x0A;       // Serial Digital Port: ADC
static const uint8_t ES8311_REG0B_SYSTEM = 0x0B;       // System
static const uint8_t ES8311_REG0C_SYSTEM = 0x0C;       // System
static const uint8_t ES8311_REG0D_SYSTEM = 0x0D;       // System: power up/down
static const uint8_t ES8311_REG0E_SYSTEM = 0x0E;       // System: power up/down
static const uint8_t ES8311_REG0F_SYSTEM = 0x0F;       // System: low power
static const uint8_t ES8311_REG10_SYSTEM = 0x10;       // System
static const uint8_t ES8311_REG11_SYSTEM = 0x11;       // System
static const uint8_t ES8311_REG12_SYSTEM = 0x12;       // System: Enable DAC
static const uint8_t ES8311_REG13_SYSTEM = 0x13;       // System
static const uint8_t ES8311_REG14_SYSTEM = 0x14;       // System: select DMIC, select analog pga gain
static const uint8_t ES8311_REG15_ADC = 0x15;          // ADC: adc ramp rate, dmic sense
static const uint8_t ES8311_REG16_ADC = 0x16;          // ADC
static const uint8_t ES8311_REG17_ADC = 0x17;          // ADC: volume
static const uint8_t ES8311_REG18_ADC = 0x18;          // ADC: alc enable and winsize
static const uint8_t ES8311_REG19_ADC = 0x19;          // ADC: alc maxlevel
static const uint8_t ES8311_REG1A_ADC = 0x1A;          // ADC: alc automute
static const uint8_t ES8311_REG1B_ADC = 0x1B;          // ADC: alc automute, adc hpf s1
static const uint8_t ES8311_REG1C_ADC = 0x1C;          // ADC: equalizer, hpf s2
static const uint8_t ES8311_REG31_DAC = 0x31;          // DAC: mute
static const uint8_t ES8311_REG32_DAC = 0x32;          // DAC: volume
static const uint8_t ES8311_REG33_DAC = 0x33;          // DAC: offset
static const uint8_t ES8311_REG34_DAC = 0x34;          // DAC: drc enable, drc winsize
static const uint8_t ES8311_REG35_DAC = 0x35;          // DAC: drc maxlevel, minilevel
static const uint8_t ES8311_REG37_DAC = 0x37;          // DAC: ramprate
static const uint8_t ES8311_REG44_GPIO = 0x44;         // GPIO: dac2adc for test
static const uint8_t ES8311_REG45_GP = 0x45;           // GPIO: GP control
static const uint8_t ES8311_REGFD_CHD1 = 0xFD;         // Chip: ID1
static const uint8_t ES8311_REGFE_CHD2 = 0xFE;         // Chip: ID2
static const uint8_t ES8311_REGFF_CHVER = 0xFF;        // Chip: Version

}  // namespace es8311
}  // namespace esphome
