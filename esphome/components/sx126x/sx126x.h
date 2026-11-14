#pragma once

#include "sx126x_radio.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "sx126x_reg.h"
#include <utility>
#include <vector>

namespace esphome::sx126x {

static constexpr size_t SX126X_MAX_PACKET_SIZE = 255;

class SX126xListener {
 public:
  // Called from loop(). The packet is borrowed until this returns; copy what you keep.
  // Listeners may send, change mode or configure. A send returns BUSY if another packet
  // is already waiting, which is then delivered by the next loop().
  virtual void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) = 0;
};

class SX126x final : public Component {
 public:
  void set_spi_parent(spi::SPIComponent *parent) { this->radio_.bus().set_spi_parent(parent); }
  void set_cs_pin(GPIOPin *pin) { this->radio_.bus().set_cs_pin(pin); }
  void set_data_rate(uint32_t rate) { this->radio_.bus().set_data_rate(rate); }
  void set_mode(spi::SPIMode mode) { this->radio_.bus().set_mode(mode); }
  void set_bit_order(spi::SPIBitOrder order) { this->radio_.bus().set_bit_order(order); }
  void set_release_device(bool release) { this->radio_.bus().set_release_device(release); }
  void set_write_only(bool write_only) { this->radio_.bus().set_write_only(write_only); }
#ifdef USE_SPI_PSRAM_DMA
  void set_psram_dma(bool enable) { this->radio_.bus().set_psram_dma(enable); }
#endif
  void spi_teardown() { this->radio_.bus().teardown(); }
  size_t get_max_packet_size();
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_bandwidth(SX126xBw bandwidth) { this->requested_profile_.bandwidth = bandwidth; }
  void set_bitrate(uint32_t bitrate) { this->requested_profile_.bitrate = bitrate; }
  void set_busy_pin(GPIOPin *busy_pin) { this->radio_.bus().set_busy_pin(busy_pin); }
  void set_coding_rate(uint8_t coding_rate) { this->requested_profile_.coding_rate = coding_rate; }
  void set_crc_enable(bool crc_enable) { this->requested_profile_.crc_enable = crc_enable; }
  void set_crc_inverted(bool crc_inverted) { this->requested_profile_.crc_inverted = crc_inverted; }
  void set_crc_size(uint8_t crc_size) { this->requested_profile_.crc_size = crc_size; }
  void set_crc_polynomial(uint16_t crc_polynomial) { this->requested_profile_.crc_polynomial = crc_polynomial; }
  void set_crc_initial(uint16_t crc_initial) { this->requested_profile_.crc_initial = crc_initial; }
  void set_whitening_enable(bool whitening_enable) { this->requested_profile_.whitening_enable = whitening_enable; }
  void set_whitening_initial(uint16_t whitening_initial) {
    this->requested_profile_.whitening_initial = whitening_initial;
  }
  void set_deviation(uint32_t deviation) { this->requested_profile_.deviation = deviation; }
  void set_dio1_pin(GPIOPin *dio1_pin) { this->radio_.set_dio1_pin(dio1_pin); }
  void set_frequency(uint32_t frequency) { this->requested_profile_.frequency = frequency; }
  void set_sx1261(bool sx1261) { this->requested_profile_.sx1261 = sx1261; }
  // Mode changes are ignored with a warning unless the radio is idle and configured.
  void set_mode_rx();
  void set_mode_tx();
  void set_mode_standby(SX126xStandbyMode mode);
  void set_mode_sleep(bool cold = false);
  void set_modulation(uint8_t modulation) { this->requested_profile_.modulation = modulation; }
  void set_pa_power(int8_t power) { this->requested_profile_.pa_power = power; }
  void set_pa_ramp(uint8_t ramp) { this->requested_profile_.pa_ramp = ramp; }
  void set_payload_length(uint8_t payload_length) { this->requested_profile_.payload_length = payload_length; }
  void set_preamble_detect(uint16_t preamble_detect) { this->requested_profile_.preamble_detect = preamble_detect; }
  void set_preamble_size(uint16_t preamble_size) { this->requested_profile_.preamble_size = preamble_size; }
  void set_rst_pin(GPIOPin *rst_pin) { this->radio_.set_rst_pin(rst_pin); }
  void set_rx_start(bool rx_start) { this->requested_profile_.rx_start = rx_start; }
  void set_rf_switch(bool rf_switch) { this->requested_profile_.rf_switch = rf_switch; }
  void set_shaping(uint8_t shaping) { this->requested_profile_.shaping = shaping; }
  void set_spreading_factor(uint8_t spreading_factor) { this->requested_profile_.spreading_factor = spreading_factor; }
  void set_sync_value(const std::vector<uint8_t> &sync_value) { this->requested_profile_.set_sync_value(sync_value); }
  void set_tcxo_voltage(uint8_t tcxo_voltage) { this->requested_profile_.tcxo_voltage = tcxo_voltage; }
  void set_tcxo_delay(uint32_t tcxo_delay) { this->requested_profile_.tcxo_delay = tcxo_delay; }
  void run_image_cal();
  // Mode changes and configuration are allowed during RX callbacks when the radio
  // is available; they never modify the borrowed packet buffer.
  void configure();
  SX126xError transmit_packet(const uint8_t *data, size_t len);
  SX126xError transmit_packet(const std::vector<uint8_t> &packet) {
    return this->transmit_packet(packet.data(), packet.size());
  }
  // Main-loop context only. SX126X_ERROR_NONE accepts one operation and promises one completion.
  // Data is copied into the chip FIFO before return; listener must outlive completion.
  // Pending RX rejects admission with BUSY and is delivered by a later loop().
  // Timeout: 1..262143 ms. SPI/BUSY waits are synchronous; airtime is asynchronous.
  [[nodiscard]] SX126xError start_transmit_async(SX126xTransmitListener *listener, const uint8_t *data, size_t len,
                                                 uint32_t timeout_ms = 4000);
  [[nodiscard]] SX126xError start_transmit_async(SX126xTransmitListener *listener, const std::vector<uint8_t> &packet,
                                                 uint32_t timeout_ms = 4000) {
    return this->start_transmit_async(listener, packet.data(), packet.size(), timeout_ms);
  }
  // Normal completion runs in loop(); cancellation completes synchronously in its caller.
  // The radio is released before callbacks, which may start another operation if recovery
  // succeeded. True accepts cancellation; the completion result reports cleanup failure.
  bool cancel_transmit_async(SX126xTransmitListener *listener);
  bool is_transmitting() const { return this->radio_.is_transmitting(); }
  void register_listener(SX126xListener *listener) { this->listeners_.push_back(listener); }
  Trigger<std::vector<uint8_t>, float, float> *get_packet_trigger() { return &this->packet_trigger_; }

 private:
  static void IRAM_ATTR gpio_intr(SX126x *arg) { arg->enable_loop_soon_any_context(); }
  void change_mode_(ModeCommand command, uint8_t arg = 0);
  // The only place that runs user code for a radio operation: loop scheduling, then status,
  // then the completion or packet listeners. SX126xRadio has already committed its state.
  void apply_(const RadioEffects &effects);
  void watch_configuration_();
  SX126xError start_transmit_(SX126xTransmitListener *listener, const uint8_t *data, size_t len, uint32_t timeout_ms);
  SX126xRadio radio_;
  RadioProfile requested_profile_;
  char version_[16]{};
  Trigger<std::vector<uint8_t>, float, float> packet_trigger_;
  std::vector<SX126xListener *> listeners_;
  std::vector<uint8_t> packet_;
  bool rx_dispatching_{false};
  bool cold_sleep_{false};
  bool reconfigure_pending_{false};
};

}  // namespace esphome::sx126x
