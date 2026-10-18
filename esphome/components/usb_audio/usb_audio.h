#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO) && \
    (defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3))

#include <array>
#include <atomic>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/static_task.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb/uac_host.h"
}

namespace esphome::usb_audio {

struct AudioEndpointConfig {
  uint32_t sample_rate{0};
  uint16_t bits_per_sample{0};
  uint8_t channels{0};
  bool configured{false};
};

struct USBAudioEvent {
  enum class Type : uint8_t {
    DRIVER_TX_CONNECTED,
    DRIVER_RX_CONNECTED,
    // The driver reports disconnects through the device callback, not the driver callback.
    DEVICE_DISCONNECTED,
    DEVICE_EVENT,
  };

  struct {
    uac_host_device_handle_t handle{nullptr};
    uac_host_device_event_t event{UAC_HOST_DEVICE_EVENT_TRANSFER_ERROR};
  } device{};
  struct {
    uint8_t addr{0};
    uint8_t iface_num{0};
  } driver{};
  Type type{Type::DEVICE_EVENT};
};

class USBAudioComponent : public Component {
 public:
  enum class Endpoint : uint8_t {
    SPEAKER,
    MICROPHONE,
  };

  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::BUS; }
  bool teardown() override;

  void set_connect_timeout(uint32_t timeout_ms) { this->connect_timeout_ms_ = timeout_ms; }

  void set_microphone_params(uint8_t channels, uint16_t bits, uint32_t sample_rate);
  void set_speaker_params(uint8_t channels, uint16_t bits, uint32_t sample_rate);

  bool ensure_started(Endpoint endpoint);
  void resume_microphone();
  void suspend_microphone();
  void resume_speaker();
  void suspend_speaker();

  void set_speaker_volume_level(float volume);
  void set_speaker_mute_state(bool mute_state);

  esp_err_t read_microphone(uint8_t *buffer, size_t size, size_t *bytes_read, uint32_t timeout_ms);
  esp_err_t write_speaker(const uint8_t *data, size_t length, uint32_t timeout_ms);

  /// Safe to call from any task; the speaker uses it to avoid blocking while the device is gone.
  bool speaker_connected() const { return this->speaker_connected_.load(std::memory_order_relaxed); }

 protected:
  static constexpr size_t EVENT_QUEUE_LENGTH = 64;

  void process_event_(const USBAudioEvent &event);
  void handle_driver_event_(const USBAudioEvent &event);
  void handle_device_disconnect_(uac_host_device_handle_t handle);
  void handle_device_event_(const USBAudioEvent &event);
  bool open_stream_(const USBAudioEvent &event);
  bool start_speaker_stream_();
  bool start_microphone_stream_();
  bool populate_stream_config_(uac_host_device_handle_t handle, const AudioEndpointConfig &config,
                               uac_host_stream_config_t &stream_config, const char *role);
  void log_alt_capabilities_(const char *role, const AudioEndpointConfig &config, const uac_host_dev_info_t &info,
                             uac_host_device_handle_t handle);
  void handle_disconnect_(uac_host_device_handle_t handle);
  void close_all_devices_();
  bool apply_speaker_volume_();
  bool apply_speaker_mute_();
  void drain_events_();

  static void driver_event_stub(uint8_t addr, uint8_t iface_num, uac_host_driver_event_t event, void *user_data);
  static void device_event_stub(uac_host_device_handle_t handle, uac_host_device_event_t event, void *user_data);
  static void usb_host_task_stub(void *param);
  void usb_host_task_();

  // Filled from the USB driver task, drained on the main loop.
  QueueHandle_t event_queue_{nullptr};
  uac_host_device_handle_t speaker_handle_{nullptr};
  uac_host_device_handle_t microphone_handle_{nullptr};
  uint32_t connect_timeout_ms_{5000};
  float speaker_volume_level_{1.0f};
  StaticTask host_task_;
  StaticQueue_t event_queue_struct_{};
  std::array<uint8_t, EVENT_QUEUE_LENGTH * sizeof(USBAudioEvent)> event_queue_storage_{};
  AudioEndpointConfig mic_config_{};
  AudioEndpointConfig speaker_config_{};

  // Shared between the main loop and the USB host task.
  std::atomic<bool> host_task_running_{false};
  std::atomic<bool> host_task_parked_{true};
  std::atomic<bool> speaker_connected_{false};

  uint8_t speaker_iface_{0};
  uint8_t microphone_iface_{0};
  uint8_t speaker_addr_{0};
  uint8_t microphone_addr_{0};
  bool host_installed_{false};
  bool uac_installed_{false};
  bool speaker_stream_started_{false};
  bool microphone_stream_started_{false};
  bool device_connected_{false};
  bool teardown_initiated_{false};
  bool speaker_muted_{false};
  bool pending_speaker_volume_{true};
  bool pending_speaker_mute_{true};
  bool speaker_volume_supported_{true};
  bool speaker_mute_supported_{true};
  bool speaker_volume_warned_{false};
  bool speaker_mute_warned_{false};
};

}  // namespace esphome::usb_audio

#endif
