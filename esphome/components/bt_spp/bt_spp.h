#pragma once

#ifdef USE_ESP32

#include "esphome/components/uart/uart_component.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/string_ref.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <esp_gap_bt_api.h>
#include <esp_spp_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/stream_buffer.h>

#include <atomic>
#include <cstdint>

namespace esphome::bt_spp {

/// Bluetooth Classic Serial Port Profile (SPP) client presented as a UART.
///
/// Connects to one remote device by its MAC address and reconnects when the link drops. Bluedroid calls the
/// callbacks from its own task: they only queue events and received bytes, and loop() handles everything else.
/// The SPP and GAP callbacks of Bluedroid are global, so there can be only one instance.
class BtSppComponent final : public uart::UARTComponent, public Component {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(connected)
#endif

 public:
  /// @param address The remote device, as the 48-bit value of its MAC address.
  /// @param buffer Static storage of rx_size + 1 + tx_size bytes.
  BtSppComponent(uint64_t address, uint8_t *buffer, size_t rx_size, size_t tx_size);

  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_pin_code(const char *pin_code) { this->pin_code_ = StringRef(pin_code); }
  void set_name(const char *name) { this->name_ = StringRef(name); }
  void set_enable_on_boot(bool enable) { this->enabled_ = enable; }
  void set_reconnect_interval(uint32_t ms) { this->reconnect_interval_ = ms; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::BLUETOOTH; }

  /// Connect now, and reconnect whenever the link drops.
  void connect();
  /// Close the link and stay disconnected until connect() is called.
  void disconnect();

  // uart::UARTComponent
  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override;
  size_t available_for_write() override;
  uart::UARTFlushResult flush() override;
  bool is_connected() override { return this->state_ == State::STATE_CONNECTED; }
  void load_settings(bool dump_config) override {}
  using UARTComponent::load_settings;

 protected:
  enum class State : uint8_t {
    STATE_STARTING,
    STATE_IDLE,
    STATE_DISCOVERING,
    STATE_CONNECTING,
    STATE_CONNECTED,
  };

  enum class EventType : uint8_t {
    EVENT_TYPE_INIT,
    EVENT_TYPE_DISCOVERY,
    EVENT_TYPE_CLIENT_INIT,
    EVENT_TYPE_OPEN,
    EVENT_TYPE_CLOSE,
    EVENT_TYPE_WRITE,
    EVENT_TYPE_CONGESTION,
    EVENT_TYPE_PIN_MISSING,
    EVENT_TYPE_PASSKEY_REJECTED,
    EVENT_TYPE_AUTH,
  };

  /// Passed from the Bluedroid task to loop(). `value` is a channel or a congestion flag.
  struct Event {
    EventType type;
    uint8_t status;
    uint16_t value;
    uint32_t handle;
  };

  static constexpr size_t EVENT_QUEUE_LENGTH = 16;

  void check_logger_conflict() override {}

  void handle_event_(const Event &event);
  void start_attempt_();
  void connect_channel_(uint8_t channel);
  void attempt_failed_();
  void schedule_retry_();
  void reset_link_();
  void accept_connection_(uint32_t handle);
  void reject_connection_(uint32_t handle);
  void set_state_(State state);
  void send_pending_();
  void drop_tx_(size_t len);

  // Bluedroid task
  void post_event_(EventType type, uint8_t status = 0, uint16_t value = 0, uint32_t handle = 0);
  void on_spp_event_(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
  void on_gap_event_(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
  static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
  static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

  // Bluedroid's callbacks carry no context pointer.
  static BtSppComponent *instance;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

  // Written by the Bluedroid task, read by loop().
  QueueHandle_t events_{nullptr};
  StaticQueue_t events_storage_{};
  uint8_t events_buffer_[EVENT_QUEUE_LENGTH * sizeof(Event)]{};
  StreamBufferHandle_t rx_stream_{nullptr};
  StaticStreamBuffer_t rx_stream_storage_{};
  std::atomic<uint32_t> rx_dropped_{0};
  std::atomic<uint32_t> events_dropped_{0};
  // Connection whose received bytes go into rx_stream_; 0 drops them. Set by the Bluedroid task when a
  // connection opens and none is active, corrected by loop() when it accepts or rejects that connection.
  std::atomic<uint32_t> rx_handle_{0};

  // Only touched by the main loop.
  uint8_t *rx_buffer_;
  uint8_t *tx_buffer_;
  size_t rx_size_;
  size_t tx_size_;
  size_t tx_len_{0};
  uint32_t reconnect_interval_{10000};
  uint32_t last_drop_log_{0};
  uint32_t handle_{0};
  // Handle of the outgoing connection that is not open yet, from ESP_SPP_CL_INIT_EVT.
  uint32_t pending_handle_{0};
  StringRef pin_code_;
  StringRef name_;
  esp_bd_addr_t address_{};
  State state_{State::STATE_STARTING};
  // Configured channel, or 0 to look it up with an SDP query.
  uint8_t channel_{0};
  // Channel found by the last SDP query, reused until a connection to it fails.
  uint8_t found_channel_{0};
  uint8_t peek_{0};
  bool has_peek_{false};
  bool enabled_{true};
  bool write_pending_{false};
  bool congested_{false};
  // A failed attempt is logged as a warning once, then at debug level until a connection succeeds.
  bool failure_logged_{false};
};

}  // namespace esphome::bt_spp

#endif  // USE_ESP32
