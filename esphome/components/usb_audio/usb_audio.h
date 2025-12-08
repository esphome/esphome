#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO)

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb/uac_host.h"
}

namespace esphome {
namespace usb_audio {

static constexpr uint32_t USB_AUDIO_DEFAULT_BUFFER_SIZE = 6400;

struct AudioEndpointConfig {
  uint8_t channels{0};
  uint16_t bits_per_sample{0};
  uint32_t sample_rate{0};
  uint32_t buffer_size{0};
  bool configured{false};
};

struct USBAudioEvent {
  enum class Type : uint8_t {
    DRIVER_TX_CONNECTED,
    DRIVER_RX_CONNECTED,
    DRIVER_DISCONNECTED,
    DEVICE_EVENT,
  };

  Type type{Type::DEVICE_EVENT};
  struct {
    uint8_t addr{0};
    uint8_t iface_num{0};
  } driver{};
  struct {
    uac_host_device_handle_t handle{nullptr};
    uac_host_device_event_t event{UAC_HOST_DEVICE_EVENT_TRANSFER_ERROR};
  } device{};
};

class USBAudioMicrophone;
class USBAudioSpeaker;

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
  void set_microphone_buffer_size(uint32_t size) { this->mic_config_.buffer_size = size; }
  void set_speaker_buffer_size(uint32_t size) { this->speaker_config_.buffer_size = size; }

  void set_microphone_params(uint8_t channels, uint16_t bits, uint32_t sample_rate);
  void set_speaker_params(uint8_t channels, uint16_t bits, uint32_t sample_rate);

  void set_microphone(USBAudioMicrophone *microphone) { this->microphone_ = microphone; }
  void set_speaker(USBAudioSpeaker *speaker) { this->speaker_ = speaker; }

  bool ensure_started(Endpoint endpoint);
  void resume_microphone();
  void suspend_microphone();
  void resume_speaker();
  void suspend_speaker();

  void set_speaker_volume_level(float volume);
  void set_speaker_mute_state(bool mute_state);

  esp_err_t read_microphone(uint8_t *buffer, size_t size, size_t *bytes_read, uint32_t timeout_ms);
  esp_err_t write_speaker(const uint8_t *data, size_t length, uint32_t timeout_ms);

  uint32_t get_microphone_buffer_size() const { return this->mic_config_.buffer_size; }
  uint32_t get_speaker_buffer_size() const { return this->speaker_config_.buffer_size; }

  bool device_connected() const { return this->device_connected_; }

 protected:
  USBAudioMicrophone *microphone_{nullptr};
  USBAudioSpeaker *speaker_{nullptr};

  void process_event_(const USBAudioEvent &event);
  void handle_driver_event_(const USBAudioEvent &event);
  void handle_driver_disconnect_(uint8_t addr, uint8_t iface_num);
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
  bool is_ready_(const AudioEndpointConfig &config) const { return config.configured && config.buffer_size > 0; }

  static void driver_event_stub(uint8_t addr, uint8_t iface_num, uac_host_driver_event_t event, void *user_data);
  static void device_event_stub(uac_host_device_handle_t handle, uac_host_device_event_t event, void *user_data);
  static void usb_host_task_stub(void *param);
  void usb_host_task_();

  QueueHandle_t event_queue_{nullptr};
  TaskHandle_t usb_host_task_handle_{nullptr};
  bool host_task_running_{false};
  bool host_installed_{false};
  bool uac_installed_{false};
  bool speaker_stream_started_{false};
  bool microphone_stream_started_{false};
  bool device_connected_{false};
  uac_host_device_handle_t speaker_handle_{nullptr};
  uac_host_device_handle_t microphone_handle_{nullptr};
  uint8_t speaker_iface_{0};
  uint8_t microphone_iface_{0};
  uint8_t speaker_addr_{0};
  uint8_t microphone_addr_{0};

  AudioEndpointConfig mic_config_{};
  AudioEndpointConfig speaker_config_{};

  uint32_t connect_timeout_ms_{5000};
  bool teardown_initiated_{false};

  float speaker_volume_level_{1.0f};
  bool speaker_muted_{false};
  bool pending_speaker_volume_{true};
  bool pending_speaker_mute_{true};
  bool speaker_volume_supported_{true};
  bool speaker_mute_supported_{true};
  bool speaker_volume_warned_{false};
  bool speaker_mute_warned_{false};
};

}  // namespace usb_audio
}  // namespace esphome

#endif  // defined(USE_ESP32) && defined(USE_USB_AUDIO)
