#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"

#include <algorithm>
#include <cinttypes>
#include <span>

#if defined(USE_ESP32)
#include <soc/soc_caps.h>
#if SOC_RMT_SUPPORTED
#include <driver/rmt_rx.h>
#endif  // SOC_RMT_SUPPORTED
#endif  // USE_ESP32

namespace esphome::remote_receiver {

#ifdef USE_REMOTE_RECEIVER_TIMER_SAMPLING
/// BKTIMER0 counts at the 26 MHz crystal; remote_receiver_bk72xx.cpp checks this against the SDK.
static constexpr uint32_t TIMER_TICKS_PER_US = 26;

/// BK72xx timer sampling (remote_receiver_bk72xx.cpp): a timer interrupt samples the pin and records the
/// runs of one frame into one half of buffer while the loop dispatches the frame in the other half.
struct RemoteReceiverComponentStore {
  /// Returns true when it published a frame, an overflow, or a counter read error for the loop.
  bool timer_isr();
  /// Takes one sample: the logical pin level (true for a mark) and the ticks since the previous sample.
  /// Returns true like timer_isr().
  bool sample(bool level, uint32_t elapsed);
  bool end_frame(bool level);
  void finish_run(bool level, uint32_t ticks);
  void push_run();
  /// Counts a failed counter read; saturating so a flood cannot wrap the count to zero
  void count_tick_read_error() {
    if (this->tick_read_errors != UINT16_MAX)
      this->tick_read_errors = this->tick_read_errors + 1;
  }

  int32_t *buffer{nullptr};
  int32_t *capture{nullptr};
  /// Finished frame for the loop, nullptr once the loop has dispatched it
  const int32_t *volatile ready{nullptr};
  uint32_t filter_ticks{0};
  uint32_t idle_ticks{0};
  uint32_t last_counter{0};
  uint32_t level_ticks{0};
  uint32_t run_ticks{0};
  ISRInternalGPIOPin pin;
  /// Entries per frame half; buffer_size is capped at 65535 bytes, so at most 8191
  uint16_t frame_entries{0};
  uint16_t count{0};
  volatile uint16_t ready_count{0};
  /// Counter reads that timed out before the loop next ran; saturating, cleared by the loop
  volatile uint16_t tick_read_errors{0};
  uint16_t sample_period_us{0};
  bool last_level{false};
  bool run_level{false};
  bool run_active{false};
  bool capture_active{false};
  /// The frame being captured did not fit in its half
  bool frame_overflow{false};
  volatile bool overflow{false};
  /// A finished frame was dropped because the loop was still dispatching the other half
  volatile bool busy{false};
};
#elif defined(USE_ESP8266) || defined(USE_LIBRETINY) || defined(USE_RP2) || (defined(USE_ESP32) && !SOC_RMT_SUPPORTED)
struct RemoteReceiverComponentStore {
  static void gpio_intr(RemoteReceiverComponentStore *arg);

  /// Stores pulse durations in microseconds as signed integers
  ///  * Positive values indicate high pulses (marks)
  ///  * Negative values indicate low pulses (spaces)
  volatile int32_t *buffer{nullptr};
  /// The position last written to
  volatile uint32_t buffer_write{0};
  /// The start position of the last sequence
  volatile uint32_t buffer_start{0};
  /// The position last read from
  uint32_t buffer_read{0};
  volatile uint32_t commit_micros{0};
  volatile uint32_t prev_micros{0};
  uint32_t buffer_entries{0};
  uint32_t filter_us{10};
  uint32_t idle_us{10000};
  ISRInternalGPIOPin pin;
  volatile bool commit_level{false};
  volatile bool prev_level{false};
  volatile bool overflow{false};
};
#elif defined(USE_ESP32) && SOC_RMT_SUPPORTED
/// Words in the RMT ring: one slot per pending rmt_receive, at least two (one filling while one is decoded);
/// without a buffer_size, four slots cover a few frames queued across a stalled loop pass.
constexpr uint32_t rmt_buffer_words(uint32_t receive_symbols, uint32_t buffer_size) {
  const uint32_t slot = sizeof(rmt_rx_done_event_data_t) + receive_symbols * sizeof(rmt_symbol_word_t);
  return ((buffer_size == 0 ? slot * 4 : std::max(slot * 2, buffer_size)) + 3) / 4;
}

struct RemoteReceiverComponentStore {
  /// Stores RMT symbols and rx done event data
  volatile uint8_t *buffer{nullptr};
  /// The position last written to
  volatile uint32_t buffer_write{0};
  /// The position last read from
  volatile uint32_t buffer_read{0};
  bool overflow{false};
  uint32_t buffer_size{0};
  uint32_t receive_size{0};
  uint32_t filter_symbols{0};
  esp_err_t error{ESP_OK};
  rmt_receive_config_t config;
};
#endif

class RemoteReceiverComponent final : public remote_base::RemoteReceiverBase,
                                      public Component
#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
    ,
                                      public remote_base::RemoteRMTChannel
#endif

{
 public:
  RemoteReceiverComponent(InternalGPIOPin *pin) : RemoteReceiverBase(pin) {}
  void setup() override;
  void dump_config() override;
  void loop() override;

#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
  void set_filter_symbols(uint32_t filter_symbols) { this->filter_symbols_ = filter_symbols; }
  void set_receive_symbols(uint32_t receive_symbols) { this->receive_symbols_ = receive_symbols; }
  void set_with_dma(bool with_dma) { this->with_dma_ = with_dma; }
  void set_carrier_duty_percent(uint8_t carrier_duty_percent) { this->carrier_duty_percent_ = carrier_duty_percent; }
  void set_carrier_frequency(uint32_t carrier_frequency) { this->carrier_frequency_ = carrier_frequency; }
#endif
#ifdef USE_REMOTE_RECEIVER_TIMER_SAMPLING
  void set_filter_us(uint32_t filter_us) { this->store_.filter_ticks = filter_us * TIMER_TICKS_PER_US; }
  void set_idle_us(uint32_t idle_us) { this->store_.idle_ticks = idle_us * TIMER_TICKS_PER_US; }
  void set_sample_period_us(uint16_t sample_period_us) { this->store_.sample_period_us = sample_period_us; }
  /// Two frame halves of frame_entries each, a static array sized at code generation.
  void set_frame_buffer(int32_t *buffer, uint16_t frame_entries) {
    this->store_.buffer = buffer;
    this->store_.frame_entries = frame_entries;
  }
#else
#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
  /// uint32_t words keep the RMT symbols aligned
  void set_buffer(std::span<uint32_t> buffer) {
    this->store_.buffer = reinterpret_cast<uint8_t *>(buffer.data());
    this->store_.buffer_size = buffer.size_bytes();
  }
#elif defined(USE_ESP8266) || defined(USE_LIBRETINY) || defined(USE_RP2) || defined(USE_ESP32)
  void set_buffer(std::span<int32_t> buffer) {
    this->store_.buffer = buffer.data();
    this->store_.buffer_entries = buffer.size();
  }
#endif
  void set_filter_us(uint32_t filter_us) { this->filter_us_ = filter_us; }
  void set_idle_us(uint32_t idle_us) { this->idle_us_ = idle_us; }
#endif

 protected:
#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
  void decode_rmt_(rmt_symbol_word_t *item, size_t item_count);
  // log the failed RMT call and mark the component failed
  void fail_(esp_err_t error, const LogString *reason);
  rmt_channel_handle_t channel_{NULL};
  uint32_t filter_symbols_{0};
  uint32_t receive_symbols_{0};
  uint32_t carrier_frequency_{0};
  bool with_dma_{false};
  uint8_t carrier_duty_percent_{100};
#endif

#if defined(USE_REMOTE_RECEIVER_TIMER_SAMPLING) || defined(USE_ESP8266) || defined(USE_LIBRETINY) || \
    defined(USE_RP2) || defined(USE_ESP32)
  RemoteReceiverComponentStore store_;
#endif

#ifdef USE_REMOTE_RECEIVER_TIMER_SAMPLING
  static void sample_timer_isr(uint8_t channel);
#elif defined(USE_ESP8266) || defined(USE_LIBRETINY) || defined(USE_RP2) || (defined(USE_ESP32) && !SOC_RMT_SUPPORTED)
  HighFrequencyLoopRequester high_freq_;
#endif

#ifndef USE_REMOTE_RECEIVER_TIMER_SAMPLING
  uint32_t filter_us_{10};
  uint32_t idle_us_{10000};
#endif
};

}  // namespace esphome::remote_receiver
