#pragma once

#include <cstdint>
#include <span>
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "opentherm_base.h"

#ifdef USE_ESP32
#include <soc/soc_caps.h>
#endif

#if defined(USE_ESP32) && SOC_RMT_SUPPORTED

#include "driver/rmt_encoder.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::opentherm {

/**
 * RMT transport. The peripheral timestamps every edge of a response and generates the request waveform from a
 * prepared symbol array, so neither direction depends on interrupt timing.
 */
class OpenThermRMT : public OpenThermBase, public remote_base::RemoteRMTChannel {
 public:
  OpenThermRMT(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, int32_t device_timeout = 800)
      : OpenThermBase(in_pin, out_pin, device_timeout) {}

  /// Symbols the receive capture writes into, one uint32_t word each, sized from rmt_symbols at code generation.
  void set_buffer(std::span<uint32_t> buffer) { this->buffer_ = buffer; }

  bool initialize() override;
  void listen() override;
  void loop() override;

  static bool rx_done_isr(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_ctx);
  static bool tx_done_isr(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_ctx);

 private:
  void write_frame() override;
  void abort() override;

  bool init_rx_();
  bool init_tx_();
  void arm_rx_();
  void disarm_rx_();
  void write_idle_();
  void decode_rx_();
  void build_frame_();

  rmt_channel_handle_t rx_{nullptr};
  rmt_channel_handle_t tx_{nullptr};
  rmt_encoder_handle_t encoder_{nullptr};
  std::span<uint32_t> buffer_{};

  // A 34-bit frame has one mid-bit transition per bit, so at most 68 edges plus the leading idle.
  static constexpr size_t RMT_MAX_EDGES = 80;

  rmt_symbol_word_t tx_symbols_[FRAME_BITS]{};
  uint32_t edge_us_[RMT_MAX_EDGES]{};
  uint8_t edge_level_[RMT_MAX_EDGES]{};
  volatile size_t symbol_count_{0};
  rmt_receive_config_t rx_config_{};
  volatile bool rx_done_{false};
  volatile bool rx_armed_{false};
};

}  // namespace esphome::opentherm

#endif  // USE_ESP32 && SOC_RMT_SUPPORTED
