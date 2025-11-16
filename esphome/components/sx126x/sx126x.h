#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "sx126x_reg.h"
#include <utility>
#include <vector>

namespace esphome {
namespace sx126x {

enum SX126xBw : uint8_t {
  // FSK
  SX126X_BW_4800,
  SX126X_BW_5800,
  SX126X_BW_7300,
  SX126X_BW_9700,
  SX126X_BW_11700,
  SX126X_BW_14600,
  SX126X_BW_19500,
  SX126X_BW_23400,
  SX126X_BW_29300,
  SX126X_BW_39000,
  SX126X_BW_46900,
  SX126X_BW_58600,
  SX126X_BW_78200,
  SX126X_BW_93800,
  SX126X_BW_117300,
  SX126X_BW_156200,
  SX126X_BW_187200,
  SX126X_BW_234300,
  SX126X_BW_312000,
  SX126X_BW_373600,
  SX126X_BW_467000,
  // LORA
  SX126X_BW_7810,
  SX126X_BW_10420,
  SX126X_BW_15630,
  SX126X_BW_20830,
  SX126X_BW_31250,
  SX126X_BW_41670,
  SX126X_BW_62500,
  SX126X_BW_125000,
  SX126X_BW_250000,
  SX126X_BW_500000,
};

enum class SX126xError { NONE = 0, TIMEOUT, INVALID_PARAMS };

class SX126xListener {
 public:
  virtual void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) = 0;
};

class SX126x : public Component,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_8MHZ> {
 public:
  size_t get_max_packet_size();
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_bandwidth(SX126xBw bandwidth) { this->bandwidth_ = bandwidth; }
  void set_bitrate(uint32_t bitrate) { this->bitrate_ = bitrate; }
  void set_busy_pin(GPIOPin *busy_pin) { this->busy_pin_ = busy_pin; }
  void set_coding_rate(uint8_t coding_rate) { this->coding_rate_ = coding_rate; }
  void set_crc_enable(bool crc_enable) { this->crc_enable_ = crc_enable; }
  void set_crc_inverted(bool crc_inverted) { this->crc_inverted_ = crc_inverted; }
  void set_crc_size(uint8_t crc_size) { this->crc_size_ = crc_size; }
  void set_crc_polynomial(uint16_t crc_polynomial) { this->crc_polynomial_ = crc_polynomial; }
  void set_crc_initial(uint16_t crc_initial) { this->crc_initial_ = crc_initial; }
  void set_deviation(uint32_t deviation) { this->deviation_ = deviation; }
  void set_dio1_pin(GPIOPin *dio1_pin) { this->dio1_pin_ = dio1_pin; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_hw_version(const std::string &hw_version) { this->hw_version_ = hw_version; }
  void set_mode_rx();
  void set_mode_tx();
  void set_mode_standby(SX126xStandbyMode mode);
  void set_mode_sleep();
  void set_modulation(uint8_t modulation) { this->modulation_ = modulation; }
  void set_pa_power(int8_t power) { this->pa_power_ = power; }
  void set_pa_ramp(uint8_t ramp) { this->pa_ramp_ = ramp; }
  void set_payload_length(uint8_t payload_length) { this->payload_length_ = payload_length; }
  void set_preamble_detect(uint16_t preamble_detect) { this->preamble_detect_ = preamble_detect; }
  void set_preamble_size(uint16_t preamble_size) { this->preamble_size_ = preamble_size; }
  void set_rst_pin(GPIOPin *rst_pin) { this->rst_pin_ = rst_pin; }
  void set_rx_start(bool rx_start) { this->rx_start_ = rx_start; }
  void set_rf_switch(bool rf_switch) { this->rf_switch_ = rf_switch; }
  void set_shaping(uint8_t shaping) { this->shaping_ = shaping; }
  void set_spreading_factor(uint8_t spreading_factor) { this->spreading_factor_ = spreading_factor; }
  void set_sync_value(const std::vector<uint8_t> &sync_value) { this->sync_value_ = sync_value; }
  void set_tcxo_voltage(uint8_t tcxo_voltage) { this->tcxo_voltage_ = tcxo_voltage; }
  void set_tcxo_delay(uint32_t tcxo_delay) { this->tcxo_delay_ = tcxo_delay; }
  void run_image_cal();
  void configure();
  SX126xError transmit_packet(const std::vector<uint8_t> &packet);
  void register_listener(SX126xListener *listener) { this->listeners_.push_back(listener); }
  Trigger<std::vector<uint8_t>, float, float> *get_packet_trigger() const { return this->packet_trigger_; };

 protected:
  void configure_fsk_ook_();
  void configure_lora_();
  void set_packet_params_(uint8_t payload_length);
  uint8_t read_fifo_(uint8_t offset, std::vector<uint8_t> &packet);
  void write_fifo_(uint8_t offset, const std::vector<uint8_t> &packet);
  void write_opcode_(uint8_t opcode, uint8_t *data, uint8_t size);
  uint8_t read_opcode_(uint8_t opcode, uint8_t *data, uint8_t size);
  void write_register_(uint16_t reg, uint8_t *data, uint8_t size);
  void read_register_(uint16_t reg, uint8_t *data, uint8_t size);
  void call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr);
  void wait_busy_();
  Trigger<std::vector<uint8_t>, float, float> *packet_trigger_{new Trigger<std::vector<uint8_t>, float, float>()};
  std::vector<SX126xListener *> listeners_;
  std::vector<uint8_t> packet_;
  std::vector<uint8_t> sync_value_;
  GPIOPin *busy_pin_{nullptr};
  GPIOPin *dio1_pin_{nullptr};
  GPIOPin *rst_pin_{nullptr};
  std::string hw_version_;
  char version_[16];
  SX126xBw bandwidth_{SX126X_BW_125000};
  uint32_t bitrate_{0};
  bool crc_enable_{false};
  bool crc_inverted_{false};
  uint8_t crc_size_{0};
  uint16_t crc_polynomial_{0};
  uint16_t crc_initial_{0};
  uint32_t deviation_{0};
  uint32_t frequency_{0};
  uint32_t payload_length_{0};
  uint32_t tcxo_delay_{0};
  uint16_t preamble_detect_{0};
  uint16_t preamble_size_{0};
  uint8_t tcxo_voltage_{0};
  uint8_t coding_rate_{0};
  uint8_t modulation_{PACKET_TYPE_LORA};
  uint8_t pa_ramp_{0};
  uint8_t shaping_{0};
  uint8_t spreading_factor_{0};
  int8_t pa_power_{0};
  bool rx_start_{false};
  bool rf_switch_{false};
};

}  // namespace sx126x
}  // namespace esphome
