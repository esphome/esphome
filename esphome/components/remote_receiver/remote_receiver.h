#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"

#include <cinttypes>

#if defined(USE_ESP32)
#include <soc/soc_caps.h>
#if SOC_RMT_SUPPORTED
#include <driver/rmt_rx.h>
#endif  // SOC_RMT_SUPPORTED
#endif  // USE_ESP32

// The timer sampling backend exists only on BK72xx; the define alone (as in defines.h) selects nothing elsewhere.
#if defined(USE_BK72XX) && defined(USE_REMOTE_RECEIVER_TIMER_SAMPLING)
#define REMOTE_RECEIVER_TIMER_SAMPLING
#endif

namespace esphome::remote_receiver {

#if defined(REMOTE_RECEIVER_TIMER_SAMPLING)
/// BK72xx timer sampling (remote_receiver_bk72xx.cpp): a timer interrupt samples the pin and records the
/// runs of one frame into the capture half of buffer, while the loop dispatches the other half.
struct RemoteReceiverComponentStore {
  void timer_isr();
  void finish_run(bool level, uint32_t ticks);
  void push_run();
  /// Re-arm capture into the given buffer half; called with interrupts locked or before the timer runs.
  void reset(int32_t *capture);

  int32_t *buffer{nullptr};
  int32_t *capture{nullptr};
  uint32_t frame_entries{0};
  uint32_t filter_ticks{0};
  uint32_t idle_ticks{0};
  uint32_t last_counter{0};
  uint32_t level_ticks{0};
  uint32_t run_ticks{0};
  ISRInternalGPIOPin pin;
  /// Durations in capture, handed to the loop together with frame_ready
  volatile uint32_t count{0};
  bool last_level{false};
  bool run_level{false};
  bool run_active{false};
  bool capture_active{false};
  volatile bool frame_ready{false};
  volatile bool overflow{false};
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
  void set_buffer_size(uint32_t buffer_size) { this->buffer_size_ = buffer_size; }
  void set_filter_us(uint32_t filter_us) { this->filter_us_ = filter_us; }
  void set_idle_us(uint32_t idle_us) { this->idle_us_ = idle_us; }
#ifdef REMOTE_RECEIVER_TIMER_SAMPLING
  void set_sample_period_us(uint32_t sample_period_us) { this->sample_period_us_ = sample_period_us; }
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

#if defined(USE_ESP8266) || defined(USE_LIBRETINY) || defined(USE_RP2) || defined(USE_ESP32)
  RemoteReceiverComponentStore store_;
#endif

#ifdef REMOTE_RECEIVER_TIMER_SAMPLING
  void fail_(const LogString *reason);
  uint32_t sample_period_us_{100};
#elif defined(USE_ESP8266) || defined(USE_LIBRETINY) || defined(USE_RP2) || (defined(USE_ESP32) && !SOC_RMT_SUPPORTED)
  HighFrequencyLoopRequester high_freq_;
#endif

  uint32_t buffer_size_{};  // 0 on RMT targets: sized from receive_symbols in setup()
  uint32_t filter_us_{10};
  uint32_t idle_us_{10000};
};

}  // namespace esphome::remote_receiver
