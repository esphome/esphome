#pragma once

#include "sx1231_reg.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <vector>

namespace esphome::sx1231 {

static constexpr size_t SX1231_MAX_PACKET_SIZE = 64;  // the 66 byte FIFO also holds the length byte
static constexpr size_t SX1231_MAX_SYNC_SIZE = 8;

enum SX1231Bw : uint8_t {
  SX1231_BW_2_6,
  SX1231_BW_3_1,
  SX1231_BW_3_9,
  SX1231_BW_5_2,
  SX1231_BW_6_3,
  SX1231_BW_7_8,
  SX1231_BW_10_4,
  SX1231_BW_12_5,
  SX1231_BW_15_6,
  SX1231_BW_20_8,
  SX1231_BW_25_0,
  SX1231_BW_31_3,
  SX1231_BW_41_7,
  SX1231_BW_50_0,
  SX1231_BW_62_5,
  SX1231_BW_83_3,
  SX1231_BW_100_0,
  SX1231_BW_125_0,
  SX1231_BW_166_7,
  SX1231_BW_200_0,
  SX1231_BW_250_0,
  SX1231_BW_500_0,
};

enum class SX1231Error {
  SX1231_ERROR_NONE = 0,
  SX1231_ERROR_TIMEOUT,
  SX1231_ERROR_INVALID_PARAMS,
  SX1231_ERROR_NOT_READY,
  SX1231_ERROR_BUSY,
  SX1231_ERROR_CANCELLED,
};

class SX1231TransmitListener {
 public:
  virtual void on_transmit_complete(SX1231Error result) = 0;
};

// Driver state, not the chip's mode
enum class RadioState : uint8_t {
  // Before setup or after a failed mode change; configure() is retried every second
  RADIO_STATE_NEEDS_CONFIGURATION,
  // Receiving or sleeping as configured, or sending after the TX mode action (untracked)
  RADIO_STATE_IDLE,
  // A send is on air
  RADIO_STATE_TRANSMITTING,
};

class SX1231Listener {
 public:
  // Called from loop(); the packet is only valid until this returns. snr is always NAN.
  virtual void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) = 0;
};

class SX1231 final : public Component,
                     public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                           spi::DATA_RATE_8MHZ> {
 public:
  size_t get_max_packet_size();
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_bandwidth(SX1231Bw bandwidth) { this->bandwidth_ = bandwidth; }
  void set_bitrate(uint32_t bitrate) { this->bitrate_ = bitrate; }
  void set_bitsync(bool bitsync) { this->bitsync_ = bitsync; }
  void set_crc_enable(bool crc_enable) { this->crc_enable_ = crc_enable; }
  void set_deviation(uint32_t deviation) { this->deviation_ = deviation; }
  void set_dio0_pin(GPIOPin *dio0_pin) { this->dio0_pin_ = dio0_pin; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  // Ignored unless the radio is idle
  void set_mode_rx();
  void set_mode_tx();
  void set_mode_standby();
  void set_mode_sleep();
  void set_modulation(uint8_t modulation) { this->modulation_ = modulation; }
  void set_pa_pin(uint8_t pin) { this->pa_pin_ = pin; }
  void set_pa_power(int8_t power) { this->pa_power_ = power; }
  void set_pa_ramp(uint8_t ramp) { this->pa_ramp_ = ramp; }
  void set_packet_mode(bool packet_mode) { this->packet_mode_ = packet_mode; }
  void set_payload_length(uint8_t payload_length) { this->payload_length_ = payload_length; }
  void set_preamble_size(uint16_t preamble_size) { this->preamble_size_ = preamble_size; }
  void set_rst_pin(GPIOPin *rst_pin) { this->rst_pin_ = rst_pin; }
  void set_rx_floor(float floor) { this->rx_floor_ = floor; }
  void set_rx_start(bool start) { this->rx_start_ = start; }
  void set_shaping(uint8_t shaping) { this->shaping_ = shaping; }
  void set_sync_value(const StaticVector<uint8_t, SX1231_MAX_SYNC_SIZE> &sync_value) { this->sync_value_ = sync_value; }
  // Ignored while a send is on air
  void configure();
  SX1231Error transmit_packet(const uint8_t *data, size_t len);
  SX1231Error transmit_packet(const std::vector<uint8_t> &packet) {
    return this->transmit_packet(packet.data(), packet.size());
  }
  // On SX1231_ERROR_NONE, the listener is called once from loop()
  [[nodiscard]] SX1231Error start_transmit_async(SX1231TransmitListener *listener, const uint8_t *data, size_t len,
                                                 uint32_t timeout_ms = 4000);
  [[nodiscard]] SX1231Error start_transmit_async(SX1231TransmitListener *listener, const std::vector<uint8_t> &packet,
                                                 uint32_t timeout_ms = 4000) {
    return this->start_transmit_async(listener, packet.data(), packet.size(), timeout_ms);
  }
  // Completes the send with SX1231_ERROR_CANCELLED; false if the listener has none
  bool cancel_transmit_async(SX1231TransmitListener *listener);
  bool is_transmitting() const { return this->state_ == RadioState::RADIO_STATE_TRANSMITTING; }
  void register_listener(SX1231Listener *listener) { this->listeners_.push_back(listener); }
  // True while a packet is being received, or has been received and not yet read; sending would lose it.
  bool is_receiving();
  Trigger<std::vector<uint8_t>, float, float> *get_packet_trigger() { return &this->packet_trigger_; }

 protected:
  // Settings as last applied by configure(), used by sends and reception
  struct AppliedSettings {
    uint8_t payload_length{0};
    bool packet_mode{false};
    bool rx_start{false};
    bool high_power{false};
  };
  struct TxOperation {
    SX1231TransmitListener *listener{nullptr};
    uint32_t started_ms{0};
    uint32_t timeout_ms{0};
  };
  static void IRAM_ATTR gpio_intr(SX1231 *arg) { arg->enable_loop_soon_any_context(); }
  // Returns the failure reason, or nullptr
  const LogString *configure_radio_();
  // False if the chip did not enter the mode
  bool set_mode_(uint8_t mode);
  bool enter_rx_();
  bool enter_tx_();
  bool can_change_mode_();
  void change_mode_(uint8_t mode);
  // Puts the radio back in RX or sleep after a send
  bool restore_();
  // Retries configure() every second
  void fail_(const LogString *reason);
  bool packet_waiting_();
  void receive_();
  void restart_stalled_rx_();
  SX1231Error start_transmit_(SX1231TransmitListener *listener, const uint8_t *data, size_t len, uint32_t timeout_ms);
  // True once the send has ended, with its outcome in result
  bool poll_transmit_(uint32_t now, SX1231Error &result);
  SX1231Error finish_transmit_(SX1231Error result);
  void write_registers_(uint8_t reg, const uint8_t *data, size_t len);
  void write_register16_(uint8_t msb_reg, uint16_t value);
  void read_fifo_(std::vector<uint8_t> &packet);
  void write_register_(uint8_t reg, uint8_t value);
  void call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr);
  uint8_t read_register_(uint8_t reg);
  Trigger<std::vector<uint8_t>, float, float> packet_trigger_;
  std::vector<SX1231Listener *> listeners_;
  std::vector<uint8_t> packet_;
  TxOperation tx_;
  GPIOPin *dio0_pin_{nullptr};
  GPIOPin *rst_pin_{nullptr};
  uint32_t bitrate_;
  uint32_t deviation_;
  uint32_t frequency_;
  float rx_floor_;
  uint16_t preamble_size_;
  StaticVector<uint8_t, SX1231_MAX_SYNC_SIZE> sync_value_;
  AppliedSettings applied_;
  SX1231Bw bandwidth_;
  uint8_t modulation_;
  uint8_t pa_pin_;
  int8_t pa_power_;
  uint8_t pa_ramp_;
  uint8_t payload_length_;
  uint8_t shaping_;
  bool bitsync_{false};
  bool crc_enable_{false};
  bool packet_mode_{false};
  bool rx_start_{false};
  bool reconfigure_pending_{false};
  RadioState state_{RadioState::RADIO_STATE_NEEDS_CONFIGURATION};
};

}  // namespace esphome::sx1231
