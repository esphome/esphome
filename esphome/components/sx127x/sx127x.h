#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome {
namespace sx127x {

enum SX127xReg : uint8_t {
  REG_FIFO = 0x00,
  REG_OP_MODE = 0x01,
  REG_BITRATE_MSB = 0x02,
  REG_BITRATE_LSB = 0x03,
  REG_FDEV_MSB = 0x04,
  REG_FDEV_LSB = 0x05,
  REG_FRF_MSB = 0x06,
  REG_FRF_MID = 0x07,
  REG_FRF_LSB = 0x08,
  REG_PA_CONFIG = 0x09,
  REG_PA_RAMP = 0x0A,
  REG_RX_CONFIG = 0x0D,
  REG_RSSI_THRESH = 0x10,
  REG_RX_BW = 0x12,
  REG_OOK_PEAK = 0x14,
  REG_OOK_FIX = 0x15,
  REG_OOK_AVG = 0x16,
  REG_AFC_FEI = 0x1A,
  REG_PREAMBLE_DETECT = 0x1F,
  REG_PREAMBLE_MSB = 0x25,
  REG_PREAMBLE_LSB = 0x26,
  REG_SYNC_CONFIG = 0x27,
  REG_SYNC_VALUE1 = 0x28,
  REG_SYNC_VALUE2 = 0x29,
  REG_SYNC_VALUE3 = 0x2A,
  REG_SYNC_VALUE4 = 0x2B,
  REG_SYNC_VALUE5 = 0x2C,
  REG_SYNC_VALUE6 = 0x2D,
  REG_SYNC_VALUE7 = 0x2E,
  REG_SYNC_VALUE8 = 0x2F,
  REG_PACKET_CONFIG_1 = 0x30,
  REG_PACKET_CONFIG_2 = 0x31,
  REG_PAYLOAD_LENGTH = 0x32,
  REG_FIFO_THRESH = 0x35,
  REG_DIO_MAPPING1 = 0x40,
  REG_DIO_MAPPING2 = 0x41,
  REG_VERSION = 0x42
};

enum SX127xRxConfig : uint8_t {
  RESTART_NO_LOCK = 0x40,
  RESTART_PLL_LOCK = 0x20,
  AFC_AUTO_ON = 0x10,
  AGC_AUTO_ON = 0x08,
  TRIGGER_NONE = 0x00,
  TRIGGER_RSSI = 0x01,
  TRIGGER_PREAMBLE = 0x06,
  TRIGGER_ALL = 0x07,
};

enum SX127xFifoThresh : uint8_t {
  TX_START_FIFO_EMPTY = 0x80,
  TX_START_FIFO_LEVEL = 0x00,
};

enum SX127xAfcFei : uint8_t {
  AFC_AUTO_CLEAR_ON = 0x01,
};

enum SX127xSyncConfig : uint8_t {
  AUTO_RESTART_OFF = 0x00,
  AUTO_RESTART_NO_LOCK = 0x80,
  AUTO_RESTART_PLL_LOCK = 0x40,
  PREAMBLE_AA = 0x00,
  PREAMBLE_55 = 0x20,
  SYNC_OFF = 0x00,
  SYNC_ON = 0x10,
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

enum SX127xDioMapping1 : uint8_t {
  DIO0_MAPPING_00 = 0x00,
  DIO0_MAPPING_01 = 0x40,
  DIO0_MAPPING_10 = 0x80,
  DIO0_MAPPING_11 = 0xC0,
  DIO1_MAPPING_00 = 0x00,
  DIO1_MAPPING_01 = 0x10,
  DIO1_MAPPING_10 = 0x20,
  DIO1_MAPPING_11 = 0x30,
  DIO2_MAPPING_00 = 0x00,
  DIO2_MAPPING_01 = 0x04,
  DIO2_MAPPING_10 = 0x08,
  DIO2_MAPPING_11 = 0x0C,
};

enum SX127xPreambleDetect : uint8_t {
  PREAMBLE_DETECTOR_ON = 0x80,
  PREAMBLE_DETECTOR_OFF = 0x00,
  PREAMBLE_BYTES_1 = 0x00,
  PREAMBLE_BYTES_2 = 0x20,
  PREAMBLE_BYTES_3 = 0x40,
  PREAMBLE_BYTES_SHIFT = 0x05,
};

enum SX127xDioMapping2 : uint8_t {
  MAP_PREAMBLE_INT = 0x01,
  MAP_RSSI_INT = 0x00,
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

enum SX127xPacketConfig2 : uint8_t {
  CONTINUOUS_MODE = 0x00,
  PACKET_MODE = 0x40,
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
  CUTOFF_BR_X_2 = 0x40,
  CUTOFF_BR_X_1 = 0x20,
  GAUSSIAN_BT_0_3 = 0x60,
  GAUSSIAN_BT_0_5 = 0x40,
  GAUSSIAN_BT_1_0 = 0x20,
  NO_SHAPING = 0x00,
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

struct SX127xStore {
  static void gpio_intr(SX127xStore *arg);
  volatile uint32_t dio0_micros{0};
  volatile bool dio0_irq{false};
  volatile bool dio2_toggle{false};
  ISRInternalGPIOPin dio2_pin;
};

enum SX127xPaConfig : uint8_t { PA_PIN_RFO = 0x00, PA_PIN_BOOST = 0x80, PA_MAX_POWER = 0x70 };

class SX127x : public Component,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_8MHZ> {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_dio0_pin(InternalGPIOPin *dio0_pin) { this->dio0_pin_ = dio0_pin; }
  void set_dio2_pin(InternalGPIOPin *dio2_pin) { this->dio2_pin_ = dio2_pin; }
  void set_rst_pin(InternalGPIOPin *rst_pin) { this->rst_pin_ = rst_pin; }
  void set_nss_pin(InternalGPIOPin *nss_pin) { this->nss_pin_ = nss_pin; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_bitrate(uint32_t bitrate) { this->bitrate_ = bitrate; }
  void set_modulation(SX127xOpMode modulation) { this->modulation_ = modulation; }
  void set_shaping(SX127xPaRamp shaping) { this->shaping_ = shaping; }
  void set_fsk_ramp(SX127xPaRamp ramp) { this->fsk_ramp_ = ramp; }
  void set_fsk_fdev(uint32_t fdev) { this->fsk_fdev_ = fdev; }
  void set_rx_start(bool start) { this->rx_start_ = start; }
  void set_rx_floor(float floor) { this->rx_floor_ = floor; }
  void set_rx_bandwidth(SX127xRxBw bandwidth) { this->rx_bandwidth_ = bandwidth; }
  void set_rx_duration(uint32_t duration) { this->rx_duration_ = duration; }
  void set_pa_pin(SX127xPaConfig pin) { this->pa_pin_ = pin; }
  void set_pa_power(uint32_t power) { this->pa_power_ = power; }
  void set_sync_value(const std::vector<uint8_t> &sync_value) { this->sync_value_ = sync_value; }
  void set_payload_length(uint8_t payload_length) { this->payload_length_ = payload_length; }
  void set_preamble_polarity(uint8_t preamble_polarity) { this->preamble_polarity_ = preamble_polarity; }
  void set_preamble_size(uint8_t preamble_size) { this->preamble_size_ = preamble_size; }
  void set_preamble_errors(uint8_t preamble_errors) { this->preamble_errors_ = preamble_errors; }
  void set_mode_standby();
  void set_mode_tx();
  void set_mode_rx();
  void configure();
  void transmit_packet(const std::vector<uint8_t> &packet);
  Trigger<std::vector<uint8_t>> *get_packet_trigger() const { return this->packet_trigger_; };

 protected:
  void write_fifo_(const std::vector<uint8_t> &packet);
  void read_fifo_(std::vector<uint8_t> &packet);
  void write_register_(uint8_t reg, uint8_t value);
  uint8_t single_transfer_(uint8_t reg, uint8_t value);
  uint8_t read_register_(uint8_t reg);
  Trigger<std::vector<uint8_t>> *packet_trigger_{new Trigger<std::vector<uint8_t>>()};
  std::vector<uint8_t> sync_value_;
  InternalGPIOPin *dio0_pin_{nullptr};
  InternalGPIOPin *dio2_pin_{nullptr};
  InternalGPIOPin *rst_pin_{nullptr};
  InternalGPIOPin *nss_pin_{nullptr};
  SX127xStore store_;
  SX127xPaConfig pa_pin_;
  SX127xRxBw rx_bandwidth_;
  SX127xOpMode modulation_;
  SX127xPaRamp shaping_;
  SX127xPaRamp fsk_ramp_;
  uint32_t fsk_fdev_;
  uint32_t frequency_;
  uint32_t bitrate_;
  uint32_t rx_duration_;
  uint32_t pa_power_;
  uint32_t payload_length_;
  uint8_t preamble_polarity_;
  uint8_t preamble_size_;
  uint8_t preamble_errors_;
  uint8_t rx_config_;
  float rx_floor_;
  bool rx_start_;
};

}  // namespace sx127x
}  // namespace esphome
