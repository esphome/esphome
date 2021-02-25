#pragma once
#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace sx1276 {
#define REG_FIFO 0x00
#define REG_VERSION 0x42
#define REG_OP_MODE 0x01
#define REG_FRF_MSB 0x06
#define REG_FRF_MID 0x07
#define REG_FRF_LSB 0x08
#define REG_PA_CONFIG 0x09
#define REG_LNA 0x0c
#define REG_FIFO_ADDR_PTR 0x0d
#define REG_FIFO_TX_BASE_ADDR 0x0e
#define REG_FIFO_RX_BASE_ADDR 0x0f
#define REG_IRQ_FLAGS 0x12
#define REG_MODEM_CONFIG_1 0x1d
#define REG_MODEM_CONFIG_2 0x1e
#define REG_PAYLOAD_LENGTH 0x22
#define REG_MODEM_CONFIG_3 0x26
#define REG_DETECTION_OPTIMIZE 0x31
#define REG_DETECTION_THRESHOLD 0x37
#define REG_SYNC_WORD 0x39
#define REG_PaDac 0x4d

// modes
#define MODE_LONG_RANGE_MODE 0x80
#define MODE_SLEEP 0x00
#define MODE_STDBY 0x01
#define MODE_TX 0x03
#define MODE_RX_CONTINUOUS 0x05

// PA config
//#define PA_BOOST                 0x80
//#define RFO                      0x70
// IRQ masks
#define IRQ_TX_DONE_MASK 0x08

/*!
 * RegPaConfig
 */
#define RF_PACONFIG_PASELECT_MASK 0x7F
#define RF_PACONFIG_PASELECT_PABOOST 0x80

#define RF_PACONFIG_MAX_POWER_MASK 0x8F

#define RF_PACONFIG_OUTPUTPOWER_MASK 0xF0

/*!
 * RegPaDac
 */
#define RF_PADAC_20DBM_MASK 0xF8
#define RF_PADAC_20DBM_ON 0x07
#define RF_PADAC_20DBM_OFF 0x04  // Default

#define MAX_PKT_LENGTH 255

class SX1276 : public sensor::Sensor,
               public PollingComponent,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_8MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void set_di0_pin(GPIOPin *di0_pin) { this->di0_pin_ = di0_pin; }
  void set_rst_pin(GPIOPin *rst_pin) { this->rst_pin_ = rst_pin; }
  void set_band(uint16_t band) { this->band_ = band; }
  float get_setup_priority() const override { return setup_priority::DATA; }
  uint8_t read_register(uint8_t address);
  void write_register(uint8_t address, uint8_t value);
  uint8_t single_transfer(uint8_t address, uint8_t value);
  void set_frequency();
  void sleep();
  void set_tx_power(int8_t power, int8_t output_pin);
  void set_signal_bandwidth(long sbw);
  void set_spreading_factor(int sf);
  void set_sync_word(int sw);
  void enable_crc();
  void disable_crc();
  void idle();
  void explicit_header_mode();
  void implicit_header_mode();
  int begin_packet(int implicit_header = false);
  int end_packet(bool async = false);

  size_t write(const char *buffer, int size);
  void send_text_printf(const char *format, ...) __attribute__((format(printf, 2, 3)));
  void receive(int size = 0);

 protected:
  GPIOPin *di0_pin_{nullptr};
  GPIOPin *rst_pin_{nullptr};
  uint16_t band_;
  uint8_t implicit_header_mode_;
  unsigned int counter_ = 0;
};
}  // namespace sx1276
}  // namespace esphome