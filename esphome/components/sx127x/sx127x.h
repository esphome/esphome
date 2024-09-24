#pragma once

#include "esphome/components/spi/spi.h"

namespace esphome {
namespace sx127x {

enum SX127xReg : uint8_t {
  REG_OP_MODE = 0x01,
  REG_FDEV_MSB = 0x04,
  REG_FDEV_LSB = 0x05,
  REG_FRF_MSB = 0x06,
  REG_FRF_MID = 0x07,
  REG_FRF_LSB = 0x08,
  REG_PA_CONFIG = 0x09,
  REG_PA_RAMP = 0x0A,
  REG_RX_BW = 0x12,
  REG_OOK_PEAK = 0x14,
  REG_OOK_FIX = 0x15,
  REG_OOK_AVG = 0x16,
  REG_SYNC_CONFIG = 0x27,
  REG_PACKET_CONFIG_1 = 0x30,
  REG_PACKET_CONFIG_2 = 0x31,
  REG_VERSION = 0x42
};

enum SX127xOpMode : uint8_t {
  MOD_FSK = 0x00,
  MOD_OOK = 0x20,
  MODE_LF_ON = 0x08,
  MODE_RX = 0x05,
  MODE_RX_FS = 0x04,
  MODE_TX = 0x03,
  MODE_TX_FS = 0x02,
  MODE_STDBY = 0x01,
  MODE_SLEEP = 0x00
};

enum SX127xOokPeak : uint8_t {
  BIT_SYNC_ON = 0x20,
  BIT_SYNC_OFF = 0x00,
  OOK_THRESH_AVG = 0x10,
  OOK_THRESH_PEAK = 0x08,
  OOK_THRESH_FIXED = 0x00,
  OOK_THRESH_STEP_6_0 = 0x07,
  OOK_THRESH_STEP_5_0 = 0x06,
  OOK_THRESH_STEP_4_0 = 0x05,
  OOK_THRESH_STEP_3_0 = 0x04,
  OOK_THRESH_STEP_2_0 = 0x03,
  OOK_THRESH_STEP_1_5 = 0x02,
  OOK_THRESH_STEP_1_0 = 0x01,
  OOK_THRESH_STEP_0_5 = 0x00
};

enum SX127xOokAvg : uint8_t {
  OOK_THRESH_DEC_16 = 0xE0,
  OOK_THRESH_DEC_8 = 0xC0,
  OOK_THRESH_DEC_4 = 0xA0,
  OOK_THRESH_DEC_2 = 0x80,
  OOK_THRESH_DEC_1_8 = 0x60,
  OOK_THRESH_DEC_1_4 = 0x40,
  OOK_THRESH_DEC_1_2 = 0x20,
  OOK_THRESH_DEC_1 = 0x00
};

enum SX127xRxBw : uint8_t {
  RX_BW_2_6 = 0x17,
  RX_BW_3_1 = 0x0F,
  RX_BW_3_9 = 0x07,
  RX_BW_5_2 = 0x16,
  RX_BW_6_3 = 0x0E,
  RX_BW_7_8 = 0x06,
  RX_BW_10_4 = 0x15,
  RX_BW_12_5 = 0x0D,
  RX_BW_15_6 = 0x05,
  RX_BW_20_8 = 0x14,
  RX_BW_25_0 = 0x0C,
  RX_BW_31_3 = 0x04,
  RX_BW_41_7 = 0x13,
  RX_BW_50_0 = 0x0B,
  RX_BW_62_5 = 0x03,
  RX_BW_83_3 = 0x12,
  RX_BW_100_0 = 0x0A,
  RX_BW_125_0 = 0x02,
  RX_BW_166_7 = 0x11,
  RX_BW_200_0 = 0x09,
  RX_BW_250_0 = 0x01
};

enum SX127xPaRamp : uint8_t {
  SHAPING_BT_0_3 = 0x60,
  SHAPING_BT_0_5 = 0x40,
  SHAPING_BT_1_0 = 0x20,
  SHAPING_NONE = 0x00,
  PA_RAMP_10 = 0x0F,
  PA_RAMP_12 = 0x0E,
  PA_RAMP_15 = 0x0D,
  PA_RAMP_20 = 0x0C,
  PA_RAMP_25 = 0x0B,
  PA_RAMP_31 = 0x0A,
  PA_RAMP_40 = 0x09,
  PA_RAMP_50 = 0x08,
  PA_RAMP_62 = 0x07,
  PA_RAMP_100 = 0x06,
  PA_RAMP_125 = 0x05,
  PA_RAMP_250 = 0x04,
  PA_RAMP_500 = 0x03,
  PA_RAMP_1000 = 0x02,
  PA_RAMP_2000 = 0x01,
  PA_RAMP_3400 = 0x00
};

enum SX127xPaConfig : uint8_t { PA_PIN_RFO = 0x00, PA_PIN_BOOST = 0x80, PA_MAX_POWER = 0x70 };

class SX127x : public Component,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_8MHZ> {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void set_rst_pin(InternalGPIOPin *rst_pin) { this->rst_pin_ = rst_pin; }
  void set_nss_pin(InternalGPIOPin *nss_pin) { this->nss_pin_ = nss_pin; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_modulation(SX127xOpMode modulation) { this->modulation_ = modulation; }
  void set_fsk_shaping(SX127xPaRamp shaping) { this->fsk_shaping_ = shaping; }
  void set_fsk_ramp(SX127xPaRamp ramp) { this->fsk_ramp_ = ramp; }
  void set_fsk_fdev(uint32_t fdev) { this->fsk_fdev_ = fdev; }
  void set_rx_start(bool start) { this->rx_start_ = start; }
  void set_rx_floor(float floor) { this->rx_floor_ = floor; }
  void set_rx_bandwidth(SX127xRxBw bandwidth) { this->rx_bandwidth_ = bandwidth; }
  void set_pa_pin(SX127xPaConfig pin) { this->pa_pin_ = pin; }
  void set_pa_power(uint32_t power) { this->pa_power_ = power; }
  void set_mode_standby();
  void set_mode_tx();
  void set_mode_rx();
  void configure();

 protected:
  void write_register_(uint8_t reg, uint8_t value);
  uint8_t single_transfer_(uint8_t reg, uint8_t value);
  uint8_t read_register_(uint8_t reg);
  InternalGPIOPin *rst_pin_{nullptr};
  InternalGPIOPin *nss_pin_{nullptr};
  SX127xPaConfig pa_pin_;
  SX127xRxBw rx_bandwidth_;
  SX127xOpMode modulation_;
  SX127xPaRamp fsk_shaping_;
  SX127xPaRamp fsk_ramp_;
  uint32_t fsk_fdev_;
  uint32_t frequency_;
  uint32_t pa_power_;
  float rx_floor_;
  bool rx_start_;
};

}  // namespace sx127x
}  // namespace esphome
