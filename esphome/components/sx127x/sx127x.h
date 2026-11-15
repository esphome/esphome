#pragma once

#include "sx127x_reg.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <vector>

namespace esphome::sx127x {

static constexpr size_t SX127X_MAX_PACKET_SIZE = 255;  // the payload length register is 8 bits

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

enum class SX127xError {
  SX127X_ERROR_NONE = 0,
  SX127X_ERROR_TIMEOUT,
  SX127X_ERROR_INVALID_PARAMS,
  SX127X_ERROR_NOT_READY,
  SX127X_ERROR_BUSY,
  SX127X_ERROR_CANCELLED,
};

class SX127xTransmitListener {
 public:
  virtual void on_transmit_complete(SX127xError result) = 0;
};

// Who may use the radio between operations. This is driver state, not the chip's mode.
enum class RadioState : uint8_t {
  // Before setup, or after a mode change that did not take. Sends return NOT_READY, mode changes
  // are ignored, and SX127x configures again every second.
  RADIO_STATE_NEEDS_CONFIGURATION,
  // Available. The chip is receiving or sleeping, as configured, or after the TX mode action
  // sending whatever the FIFO holds; that send is not tracked.
  RADIO_STATE_IDLE,
  // A send is on air. Mode changes, configuration and other sends are refused until it ends.
  RADIO_STATE_TRANSMITTING,
  // Standby with a received packet in the FIFO, found when a send entered standby. The send
  // was refused with BUSY; the next loop() delivers the packet and restores the radio.
  RADIO_STATE_HOLDING_RX,
};

class SX127xListener {
 public:
  // Called from loop(). The packet is borrowed until this returns; copy what you keep.
  // Listeners may send, change mode or configure. A send returns BUSY if another packet
  // is already waiting, which is then delivered by the next loop().
  virtual void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) = 0;
};

class SX127x final : public Component,
                     public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                           spi::DATA_RATE_8MHZ> {
 public:
  size_t get_max_packet_size();
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_afc_bandwidth(SX127xBw afc_bandwidth) { this->afc_bandwidth_ = afc_bandwidth; }
  void set_auto_cal(bool auto_cal) { this->auto_cal_ = auto_cal; }
  void set_bandwidth(SX127xBw bandwidth) { this->bandwidth_ = bandwidth; }
  void set_bitrate(uint32_t bitrate) { this->bitrate_ = bitrate; }
  void set_bitsync(bool bitsync) { this->bitsync_ = bitsync; }
  void set_coding_rate(uint8_t coding_rate) { this->coding_rate_ = coding_rate; }
  void set_crc_enable(bool crc_enable) { this->crc_enable_ = crc_enable; }
  void set_deviation(uint32_t deviation) { this->deviation_ = deviation; }
  void set_dio0_pin(GPIOPin *dio0_pin) { this->dio0_pin_ = dio0_pin; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  // Mode changes are ignored with a warning unless the radio is idle and configured.
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
  void set_rst_pin(GPIOPin *rst_pin) { this->rst_pin_ = rst_pin; }
  void set_rx_floor(float floor) { this->rx_floor_ = floor; }
  void set_rx_start(bool start) { this->rx_start_ = start; }
  void set_shaping(uint8_t shaping) { this->shaping_ = shaping; }
  void set_spreading_factor(uint8_t spreading_factor) { this->spreading_factor_ = spreading_factor; }
  void set_sync_value(const std::vector<uint8_t> &sync_value) { this->sync_value_ = sync_value; }
  void run_image_cal();
  // Resets the chip and applies the configuration. Ignored while a send is on air; a packet
  // held for delivery is discarded.
  void configure();
  SX127xError transmit_packet(const uint8_t *data, size_t len);
  SX127xError transmit_packet(const std::vector<uint8_t> &packet) {
    return this->transmit_packet(packet.data(), packet.size());
  }
  // Main loop context only. SX127X_ERROR_NONE starts a send that completes exactly once, through
  // the listener, which must outlive it. The data is copied into the chip FIFO before return.
  // A received packet still waiting refuses the send with BUSY, and a later loop() delivers it.
  [[nodiscard]] SX127xError start_transmit_async(SX127xTransmitListener *listener, const uint8_t *data, size_t len,
                                                 uint32_t timeout_ms = 4000);
  [[nodiscard]] SX127xError start_transmit_async(SX127xTransmitListener *listener, const std::vector<uint8_t> &packet,
                                                 uint32_t timeout_ms = 4000) {
    return this->start_transmit_async(listener, packet.data(), packet.size(), timeout_ms);
  }
  // Ends the listener's send at once with SX127X_ERROR_CANCELLED, called before this returns.
  // Returns false if that listener has no send on air.
  bool cancel_transmit_async(SX127xTransmitListener *listener);
  bool is_transmitting() const { return this->state_ == RadioState::RADIO_STATE_TRANSMITTING; }
  void register_listener(SX127xListener *listener) { this->listeners_.push_back(listener); }
  Trigger<std::vector<uint8_t>, float, float> *get_packet_trigger() { return &this->packet_trigger_; }

 protected:
  // The settings configure() last applied. Sends and reception use these, so a setter called while
  // configure() is refused, during a send, does not change them before the chip has the new value.
  struct AppliedSettings {
    uint32_t frequency{0};
    uint32_t payload_length{0};
    uint8_t modulation{0};
    bool packet_mode{false};
    bool rx_start{false};
  };
  struct TxOperation {
    SX127xTransmitListener *listener{nullptr};
    uint32_t started_ms{0};
    uint32_t timeout_ms{0};
  };
  static void IRAM_ATTR gpio_intr(SX127x *arg) { arg->enable_loop_soon_any_context(); }
  // Each returns the failure reason, or nullptr
  const LogString *configure_radio_();
  const LogString *image_cal_();
  void configure_fsk_ook_();
  void configure_lora_();
  // False if the chip did not enter the mode
  bool set_mode_(uint8_t modulation, uint8_t mode);
  bool enter_rx_();
  bool enter_tx_();
  bool can_change_mode_();
  void change_mode_(uint8_t mode);
  // Puts the radio back in RX or sleep after a send or a held packet
  bool restore_();
  // A command did not take: the radio needs configuration, which is retried every second
  void fail_(const LogString *reason);
  bool packet_waiting_();
  void receive_();
  SX127xError start_transmit_(SX127xTransmitListener *listener, const uint8_t *data, size_t len, uint32_t timeout_ms);
  // Returns true once the send has ended, with its outcome in result
  bool poll_transmit_(uint32_t now, SX127xError &result);
  SX127xError finish_transmit_(SX127xError result);
  void write_fifo_(const uint8_t *data, size_t len);
  void read_fifo_(std::vector<uint8_t> &packet);
  void write_register_(uint8_t reg, uint8_t value);
  void call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr);
  uint8_t read_register_(uint8_t reg);
  Trigger<std::vector<uint8_t>, float, float> packet_trigger_;
  std::vector<SX127xListener *> listeners_;
  std::vector<uint8_t> packet_;
  std::vector<uint8_t> sync_value_;
  AppliedSettings applied_;
  TxOperation tx_;
  GPIOPin *dio0_pin_{nullptr};
  GPIOPin *rst_pin_{nullptr};
  SX127xBw afc_bandwidth_{SX127X_BW_50_0};
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
  bool reconfigure_pending_{false};
  RadioState state_{RadioState::RADIO_STATE_NEEDS_CONFIGURATION};
};

}  // namespace esphome::sx127x
