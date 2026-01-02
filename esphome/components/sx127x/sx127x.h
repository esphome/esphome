#pragma once

#include "sx127x_reg.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include <vector>

namespace esphome {
namespace sx127x {

enum SX127xBw : uint8_t {
  SX127X_BW_2_6,
  SX127X_BW_3_1,
  SX127X_BW_3_9,
  SX127X_BW_5_2,
  SX127X_BW_6_3,
  SX127X_BW_7_8,
  SX127X_BW_10_4,
  SX127X_BW_12_5,
  SX127X_BW_15_6,
  SX127X_BW_20_8,
  SX127X_BW_25_0,
  SX127X_BW_31_3,
  SX127X_BW_41_7,
  SX127X_BW_50_0,
  SX127X_BW_62_5,
  SX127X_BW_83_3,
  SX127X_BW_100_0,
  SX127X_BW_125_0,
  SX127X_BW_166_7,
  SX127X_BW_200_0,
  SX127X_BW_250_0,
  SX127X_BW_500_0,
};

enum class SX127xError { NONE = 0, TIMEOUT, INVALID_PARAMS };

class SX127xListener {
 public:
  virtual void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) = 0;
};

class SX127x : public Component,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_8MHZ> {
 public:
  size_t get_max_packet_size();
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_auto_cal(bool auto_cal) { this->auto_cal_ = auto_cal; }
  void set_bandwidth(SX127xBw bandwidth) { this->bandwidth_ = bandwidth; }
  void set_bitrate(uint32_t bitrate) { this->bitrate_ = bitrate; }
  void set_bitsync(bool bitsync) { this->bitsync_ = bitsync; }
  void set_coding_rate(uint8_t coding_rate) { this->coding_rate_ = coding_rate; }
  void set_crc_enable(bool crc_enable) { this->crc_enable_ = crc_enable; }
  void set_deviation(uint32_t deviation) { this->deviation_ = deviation; }
  void set_dio0_pin(InternalGPIOPin *dio0_pin) { this->dio0_pin_ = dio0_pin; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_mode_rx();
  void set_mode_tx();
  void set_mode_standby();
  void set_mode_sleep();
  void set_modulation(uint8_t modulation) { this->modulation_ = modulation; }
  void set_pa_pin(uint8_t pin) { this->pa_pin_ = pin; }
  void set_pa_power(uint8_t power) { this->pa_power_ = power; }
  void set_pa_ramp(uint8_t ramp) { this->pa_ramp_ = ramp; }
  void set_packet_mode(bool packet_mode) { this->packet_mode_ = packet_mode; }
  void set_payload_length(uint8_t payload_length) { this->payload_length_ = payload_length; }
  void set_preamble_errors(uint8_t preamble_errors) { this->preamble_errors_ = preamble_errors; }
  void set_preamble_polarity(uint8_t preamble_polarity) { this->preamble_polarity_ = preamble_polarity; }
  void set_preamble_size(uint16_t preamble_size) { this->preamble_size_ = preamble_size; }
  void set_preamble_detect(uint8_t preamble_detect) { this->preamble_detect_ = preamble_detect; }
  void set_rst_pin(InternalGPIOPin *rst_pin) { this->rst_pin_ = rst_pin; }
  void set_rx_floor(float floor) { this->rx_floor_ = floor; }
  void set_rx_start(bool start) { this->rx_start_ = start; }
  void set_shaping(uint8_t shaping) { this->shaping_ = shaping; }
  void set_spreading_factor(uint8_t spreading_factor) { this->spreading_factor_ = spreading_factor; }
  void set_sync_value(const std::vector<uint8_t> &sync_value) { this->sync_value_ = sync_value; }
  void run_image_cal();
  void configure();
  SX127xError transmit_packet(const std::vector<uint8_t> &packet);
  void register_listener(SX127xListener *listener) { this->listeners_.push_back(listener); }
  Trigger<std::vector<uint8_t>, float, float> *get_packet_trigger() const { return this->packet_trigger_; };

 protected:
  void configure_fsk_ook_();
  void configure_lora_();
  void set_mode_(uint8_t modulation, uint8_t mode);
  void write_fifo_(const std::vector<uint8_t> &packet);
  void read_fifo_(std::vector<uint8_t> &packet);
  void write_register_(uint8_t reg, uint8_t value);
  void call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr);
  uint8_t read_register_(uint8_t reg);
  Trigger<std::vector<uint8_t>, float, float> *packet_trigger_{new Trigger<std::vector<uint8_t>, float, float>()};
  std::vector<SX127xListener *> listeners_;
  std::vector<uint8_t> packet_;
  std::vector<uint8_t> sync_value_;
  InternalGPIOPin *dio0_pin_{nullptr};
  InternalGPIOPin *rst_pin_{nullptr};
  SX127xBw bandwidth_;
  uint32_t bitrate_;
  uint32_t deviation_;
  uint32_t frequency_;
  uint32_t payload_length_;
  uint16_t preamble_size_;
  uint8_t coding_rate_;
  uint8_t modulation_;
  uint8_t pa_pin_;
  uint8_t pa_power_;
  uint8_t pa_ramp_;
  uint8_t preamble_detect_;
  uint8_t preamble_errors_;
  uint8_t preamble_polarity_;
  uint8_t shaping_;
  uint8_t spreading_factor_;
  float rx_floor_;
  bool auto_cal_{false};
  bool bitsync_{false};
  bool crc_enable_{false};
  bool packet_mode_{false};
  bool rx_start_{false};
};

}  // namespace sx127x
}  // namespace esphome
