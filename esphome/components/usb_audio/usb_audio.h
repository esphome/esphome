#pragma once

#include "esphome/core/defines.h"

#ifdef USE_USB_AUDIO

#include <array>
#include <atomic>

#include "esphome/components/audio/audio.h"
#include "esphome/core/component.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/uac_host.h"
}

namespace esphome::usb_audio {

enum class Endpoint : uint8_t {
  SPEAKER,
  MICROPHONE,
};

/// A speaker or microphone platform that streams through the hub. The hub calls it on the main loop only.
class USBAudioStream {
 public:
  explicit USBAudioStream(uint32_t buffer_size) : buffer_size_(buffer_size) {}

  uint32_t get_buffer_size() const { return this->buffer_size_; }
  /// The fixed format the hub opens the stream with.
  virtual audio::AudioStreamInfo get_usb_stream_info() = 0;
  /// The stream is running (true), or the device went away (false).
  virtual void on_stream_state(bool running) = 0;

 protected:
  uint32_t buffer_size_;
};

struct USBAudioEvent {
  enum class Type : uint8_t {
    CONNECTED,
    // The driver reports disconnects through the device callback, not the driver callback.
    DISCONNECTED,
  };

  union {
    uac_host_device_handle_t handle{nullptr};  // DISCONNECTED
    struct {
      uint8_t addr;
      uint8_t iface_num;
    } driver;  // CONNECTED
  };
  Type type{Type::CONNECTED};
  Endpoint endpoint{Endpoint::SPEAKER};
};

class USBAudioComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  // Just after usb_host, which installs the USB host library that uac_host_install needs.
  float get_setup_priority() const override { return setup_priority::BUS - 1.0f; }
  bool teardown() override;

  void register_speaker(USBAudioStream *speaker) { this->endpoint_(Endpoint::SPEAKER).platform = speaker; }
  void register_microphone(USBAudioStream *microphone) { this->endpoint_(Endpoint::MICROPHONE).platform = microphone; }

  /// Start streaming as soon as the device is connected; the platform hears back through on_stream_state().
  void resume(Endpoint endpoint);
  void suspend(Endpoint endpoint);

  void set_speaker_volume_level(float volume);
  void set_speaker_mute_state(bool mute_state);

  esp_err_t read_microphone(uint8_t *buffer, size_t size, size_t *bytes_read, uint32_t timeout_ms);
  esp_err_t write_speaker(const uint8_t *data, size_t length, uint32_t timeout_ms);

 protected:
  static constexpr size_t EVENT_QUEUE_LENGTH = 8;

  struct EndpointState {
    uac_host_device_handle_t handle{nullptr};
    USBAudioStream *platform{nullptr};
    uint8_t iface{0};
    bool stream_started{false};
    // The platform asked for the stream and has not stopped it.
    bool active{false};
  };

  struct ControlState {
    bool pending{false};
    bool supported{true};
    uint8_t attempts{0};
  };

  EndpointState &endpoint_(Endpoint endpoint) { return this->endpoints_[static_cast<size_t>(endpoint)]; }
  const EndpointState &endpoint_(Endpoint endpoint) const { return this->endpoints_[static_cast<size_t>(endpoint)]; }
  void process_event_(const USBAudioEvent &event);
  void open_stream_(const USBAudioEvent &event);
  void start_stream_(Endpoint endpoint);
  bool populate_stream_config_(Endpoint endpoint, uac_host_stream_config_t &stream_config);
  void handle_disconnect_(uac_host_device_handle_t handle);
  void mark_controls_pending_();
  void apply_controls_();
  void apply_control_(ControlState &control, esp_err_t err, const LogString *name);
  bool controls_pending_() const;

  static void driver_event_stub(uint8_t addr, uint8_t iface_num, uac_host_driver_event_t event, void *user_data);
  static void device_event_stub(uac_host_device_handle_t handle, uac_host_device_event_t event, void *user_data);

  // Filled from the USB driver task, drained on the main loop.
  QueueHandle_t event_queue_{nullptr};
  std::array<EndpointState, 2> endpoints_{};
  float speaker_volume_level_{1.0f};
  uint32_t last_control_attempt_ms_{0};
  // Counted on the USB driver task, logged from loop().
  std::atomic<uint32_t> transfer_errors_{0};
  StaticQueue_t event_queue_struct_{};
  std::array<uint8_t, EVENT_QUEUE_LENGTH * sizeof(USBAudioEvent)> event_queue_storage_{};
  ControlState volume_{};
  ControlState mute_{};
  bool uac_installed_{false};
  bool speaker_muted_{false};
};

}  // namespace esphome::usb_audio

#endif
