#pragma once

#include <cinttypes>

namespace esphome {
namespace es7243e {

// ES7243E register addresses
static const uint8_t ES7243E_RESET_REG00 = 0x00;      // Reset control
static const uint8_t ES7243E_CLOCK_MGR_REG01 = 0x01;  // MCLK/BCLK/ADCCLK/Analog clocks on/off
static const uint8_t ES7243E_CLOCK_MGR_REG02 = 0x02;  // MCLK & BCLK configuration, source selection

static const uint8_t ES7243E_CLOCK_MGR_REG03 = 0x03;  // ADC Over-sample rate control
static const uint8_t ES7243E_CLOCK_MGR_REG04 = 0x04;  // Pre-divide/Pre-multiplication
static const uint8_t ES7243E_CLOCK_MGR_REG05 = 0x05;  // CF/DSP clock divider
static const uint8_t ES7243E_CLOCK_MGR_REG06 = 0x06;  // BCLK divider at master mode
static const uint8_t ES7243E_CLOCK_MGR_REG07 = 0x07;  // BCLK/LRCK/SDOUT tri-state control/LRCK divider bit 11->8
static const uint8_t ES7243E_CLOCK_MGR_REG08 = 0x08;  // Master LRCK divider bit 7 to bit 0
static const uint8_t ES7243E_CLOCK_MGR_REG09 = 0x09;  // SEL S1/Timer for S1
static const uint8_t ES7243E_SDP_REG0A = 0x0A;        // SEL S3/Timer for S3
static const uint8_t ES7243E_SDP_REG0B = 0x0B;        // SDP out mute control/I2S/left-justify case/word length/format
static const uint8_t ES7243E_SDP_REG0C = 0x0C;        // NFS flag at slot0/LSB/TDM mode selection
static const uint8_t ES7243E_ADC_CTRL_REG0D = 0x0D;   // data mux/pol. inv./ram clear on lrck/mclk active/gain scale up
static const uint8_t ES7243E_ADC_CTRL_REG0E = 0x0E;   // volume control
static const uint8_t ES7243E_ADC_CTRL_REG0F = 0x0F;   // offset freeze/auto level control/automute control/VC ramp rate
static const uint8_t ES7243E_ADC_CTRL_REG10 = 0x10;   // automute noise gate/detection
static const uint8_t ES7243E_ADC_CTRL_REG11 = 0x11;   // automute SDP control/out gain select
static const uint8_t ES7243E_ADC_CTRL_REG12 = 0x12;   // controls for automute PDN_PGA/MOD/reset/digital circuit
static const uint8_t ES7243E_ADC_CTRL_REG13 = 0x13;   // ALC rate selection/ALC target level
static const uint8_t ES7243E_ADC_CTRL_REG14 = 0x14;   // ADCHPF stage1 coeff
static const uint8_t ES7243E_ADC_CTRL_REG15 = 0x15;   // ADCHPF stage2 coeff
static const uint8_t ES7243E_ANALOG_REG16 = 0x16;     // power-down/reset
static const uint8_t ES7243E_ANALOG_REG17 = 0x17;     // VMIDSEL
static const uint8_t ES7243E_ANALOG_REG18 = 0x18;     // ADC/ADCFL bias
static const uint8_t ES7243E_ANALOG_REG19 = 0x19;     // PGA1/PGA2 bias
static const uint8_t ES7243E_ANALOG_REG1A = 0x1A;     // ADCI1/ADCI23 bias
static const uint8_t ES7243E_ANALOG_REG1B = 0x1B;     // ADCSM/ADCCM bias
static const uint8_t ES7243E_ANALOG_REG1C = 0x1C;     // ADCVRP/ADCCPP bias
static const uint8_t ES7243E_ANALOG_REG1D = 0x1D;     // low power bits
static const uint8_t ES7243E_ANALOG_REG1E = 0x1E;     // low power bits
static const uint8_t ES7243E_ANALOG_REG1F = 0x1F;     // ADC_DMIC_ON/REFSEL/VX2OFF/VX1SEL/VMIDLVL
static const uint8_t ES7243E_ANALOG_REG20 = 0x20;     // select MIC1 as PGA1 input/PGA1 gain
static const uint8_t ES7243E_ANALOG_REG21 = 0x21;     // select MIC2 as PGA1 input/PGA2 gain
static const uint8_t ES7243E_TEST_MODE_REGF7 = 0xF7;
static const uint8_t ES7243E_TEST_MODE_REGF8 = 0xF8;
static const uint8_t ES7243E_TEST_MODE_REGF9 = 0xF9;
static const uint8_t ES7243E_I2C_CONF_REGFA = 0xFA;      // I2C signals retime/reset registers to default
static const uint8_t ES7243E_FLAG_REGFC = 0xFC;          // CSM flag/ADC automute flag (RO)
static const uint8_t ES7243E_CHIP_ID1_REGFD = 0xFD;      // chip ID 1, reads 0x7A (RO)
static const uint8_t ES7243E_CHIP_ID2_REGFE = 0xFE;      // chip ID 2, reads 0x43 (RO)
static const uint8_t ES7243E_CHIP_VERSION_REGFF = 0xFF;  // chip version, reads 0x00 (RO)

}  // namespace es7243e
}  // namespace esphome
