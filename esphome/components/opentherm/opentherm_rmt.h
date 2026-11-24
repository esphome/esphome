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

  static bool IRAM_ATTR HOT rx_done_isr(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata,
                                        void *user_ctx);
  static bool IRAM_ATTR HOT tx_done_isr(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata,
                                        void *user_ctx);

 private:
  bool initialize_impl_() override;
  void listen_impl_() override;
  void send_impl_() override;
  void stop_impl_() override;
  void loop_impl_() override;

  bool init_rx_();
  bool init_tx_();
  void arm_rx_();
  void decode_rx_();
  void build_frame_();

  static ProtocolErrorType decode_edges_(const uint32_t *edges_us, const uint8_t *levels, uint8_t count, uint32_t *data,
                                         uint8_t *bit_pos);

  rmt_channel_handle_t rx_{nullptr};
  rmt_channel_handle_t tx_{nullptr};
  rmt_encoder_handle_t encoder_{nullptr};
  std::span<uint32_t> buffer_{};

  // A bit is 1 ms. Manchester puts the value in the right half and its inverse in the left, so one symbol carries
  // one bit and a frame is exactly 34 symbols.
  static constexpr uint32_t HALF_BIT_US = 500;
  static constexpr size_t FRAME_BITS = 34;
  // The line is idle while the slave prepares its answer, so this has to outlast that wait.
  static constexpr uint32_t RMT_IDLE_US = 10000;
  // A 34-bit frame has one mid-bit transition per bit, so at most 68 edges plus the leading idle.
  static constexpr size_t RMT_MAX_EDGES = 80;

  rmt_symbol_word_t tx_symbols_[FRAME_BITS]{};
  uint32_t edge_us_[RMT_MAX_EDGES]{};
  uint8_t edge_level_[RMT_MAX_EDGES]{};
  volatile size_t symbol_count_{0};
  rmt_receive_config_t rx_config_{};
  volatile bool rx_done_{false};
  bool rx_armed_{false};
  uint32_t listen_start_{0};
};

}  // namespace esphome::opentherm

#endif  // USE_ESP32 && SOC_RMT_SUPPORTED
