#pragma once

#include "esphome/core/defines.h"

#ifdef USE_USB_AUDIO

#include <array>
#include <atomic>

#include "esphome/components/audio/audio.h"
#include "esphome/core/component.h"
#include "esphome/core/static_task.h"

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

/// What the hub reports to a platform about its stream.
enum class StreamEvent : uint8_t {
  // The stream runs.
  RUNNING,
  // The device went away; the hub restarts the stream when it returns while the platform still wants it.
  LOST,
  // The stream failed to start; the platform goes back to stopped and its next start() retries.
  FAILED,
};

/// A speaker or microphone platform that streams through the hub. The hub calls it on the main loop only.
class USBAudioStream {
 public:
  explicit USBAudioStream(uint32_t buffer_size) : buffer_size_(buffer_size) {}

  uint32_t get_buffer_size() const { return this->buffer_size_; }
  /// The fixed format the hub opens the stream with.
  virtual audio::AudioStreamInfo get_usb_stream_info() = 0;
  virtual void on_stream_state(StreamEvent event) = 0;
  /// No platform task is inside a read or write call on the device; the hub closes a lost device only then.
  virtual bool stream_idle() const = 0;

 protected:
  uint32_t buffer_size_;
};

/// Posted to the main loop, by the USB driver task (CONNECTED, DISCONNECTED) or the hub's worker (the results).
struct USBAudioEvent {
  enum class Type : uint8_t {
    CONNECTED,
    // The driver reports disconnects through the device callback, not the driver callback.
    DISCONNECTED,
    OPENED,
    STARTED,
    RESUMED,
    CLOSED,
  };

  uac_host_device_handle_t handle{nullptr};
  uint8_t addr{0};
  uint8_t iface_num{0};
  // RESUMED: the request it answers; only the latest request's result is acted on.
  uint8_t seq{0};
  Type type{Type::CONNECTED};
  Endpoint endpoint{Endpoint::SPEAKER};
  bool ok{false};
  // STARTED: the device does not support the configured format.
  bool format_rejected{false};
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

  /// Safe from any task; the worker sends the values to the device.
  void set_speaker_volume_level(float volume);
  void set_speaker_mute_state(bool mute_state);

  // Safe from any task. While the worker holds the driver for a control transfer, these return ESP_ERR_TIMEOUT after
  // waiting out timeout_ms instead of blocking on the driver's interface lock for up to seconds.
  esp_err_t read_microphone(uint8_t *buffer, size_t size, size_t *bytes_read, uint32_t timeout_ms);
  esp_err_t write_speaker(const uint8_t *data, size_t length, uint32_t timeout_ms);

 protected:
  static constexpr size_t EVENT_QUEUE_LENGTH = 16;
  static constexpr size_t OP_QUEUE_LENGTH = 8;

  struct EndpointState {
    // Read by the platform tasks and the worker; cleared first when the device goes away.
    std::atomic<uac_host_device_handle_t> handle{nullptr};
    // A lost device, closed by the worker once the platform reports stream_idle().
    uac_host_device_handle_t closing{nullptr};
    USBAudioStream *platform{nullptr};
    uint8_t iface{0};
    bool stream_started{false};
    // The platform asked for the stream and has not stopped it.
    bool active{false};
    // The connected device rejected the configured format; cleared when it is closed.
    bool format_rejected{false};
    // An open or start is queued on the worker; its result event clears it.
    bool open_pending{false};
    bool start_pending{false};
    bool close_queued{false};
    // Counts resume() and suspend() calls, so a RESUMED that a later suspend overtook is ignored.
    uint8_t request_seq{0};
    // A connect that arrived while the old handle was still opening or closing; opened once that finishes.
    bool connect_deferred{false};
    uint8_t deferred_addr{0};
    uint8_t deferred_iface{0};
    // The last open failed; the next resume() retries it at this address and interface.
    bool open_failed{false};
    uint8_t failed_addr{0};
    uint8_t failed_iface{0};
    // A failed close keeps `closing` and is retried after this time, a limited number of times.
    uint8_t close_attempts{0};
    uint32_t close_retry_ms{0};
  };

  struct ControlState {
    std::atomic<bool> pending{false};
    // Only touched by the worker, and reset while the speaker is closed.
    bool supported{true};
    uint8_t attempts{0};
  };

  /// A blocking driver call, run on the worker.
  struct Op {
    enum class Type : uint8_t {
      OPEN,
      START,
      RESUME,
      SUSPEND,
      CLOSE,
      CONTROLS,
    };

    uac_host_device_handle_t handle{nullptr};
    uint8_t addr{0};
    uint8_t iface_num{0};
    // RESUME: copied to the result, see EndpointState::request_seq.
    uint8_t seq{0};
    Type type{Type::OPEN};
    Endpoint endpoint{Endpoint::SPEAKER};
    // CLOSE: stop the stream before closing.
    bool stop{false};
  };

  EndpointState &endpoint_(Endpoint endpoint) { return this->endpoints_[static_cast<size_t>(endpoint)]; }
  const EndpointState &endpoint_(Endpoint endpoint) const { return this->endpoints_[static_cast<size_t>(endpoint)]; }
  void process_events_(uint32_t now);
  void process_event_(const USBAudioEvent &event, uint32_t now);
  void open_stream_(Endpoint endpoint, uint8_t addr, uint8_t iface_num);
  void on_opened_(const USBAudioEvent &event);
  void on_started_(const USBAudioEvent &event);
  void on_resumed_(const USBAudioEvent &event);
  void on_closed_(const USBAudioEvent &event, uint32_t now);
  void start_stream_(Endpoint endpoint);
  void stream_failed_(Endpoint endpoint);
  void handle_disconnect_(uac_host_device_handle_t handle);
  void begin_close_(Endpoint endpoint);
  bool finish_closes_(uint32_t now);
  bool queue_op_(const Op &op);
  void queue_controls_();
  void post_event_(const USBAudioEvent &event);

  // Worker side: the only place that makes blocking driver calls after setup().
  static void worker_task(void *param);
  void run_op_(const Op &op);
  /// Keep platform reads and writes out of the driver while the worker uses it.
  void lock_driver_();
  void unlock_driver_();
  bool populate_stream_config_(Endpoint endpoint, uac_host_device_handle_t handle,
                               uac_host_stream_config_t &stream_config, bool &format_rejected);
  /// Send pending volume and mute; returns true when one should be retried later.
  bool apply_pending_speaker_controls_();
  bool apply_control_(ControlState &control, esp_err_t err, const LogString *name);

  static void driver_event_stub(uint8_t addr, uint8_t iface_num, uac_host_driver_event_t event, void *user_data);
  static void device_event_stub(uac_host_device_handle_t handle, uac_host_device_event_t event, void *user_data);

  StaticTask worker_;
  // Filled from the USB driver task and the worker, drained on the main loop.
  QueueHandle_t event_queue_{nullptr};
  // Filled on the main loop (and by volume/mute changes from any task), drained by the worker.
  QueueHandle_t op_queue_{nullptr};
  std::array<EndpointState, 2> endpoints_{};
  // Disconnects for devices whose open is still running on the worker, one per endpoint since a speakerphone opens
  // two interfaces; each is closed once its OPENED arrives.
  std::array<uac_host_device_handle_t, 2> lost_while_opening_{};
  // Counted on the USB driver task, logged from loop().
  std::atomic<uint32_t> transfer_errors_{0};
  StaticQueue_t event_queue_struct_{};
  StaticQueue_t op_queue_struct_{};
  std::array<uint8_t, EVENT_QUEUE_LENGTH * sizeof(USBAudioEvent)> event_queue_storage_{};
  std::array<uint8_t, OP_QUEUE_LENGTH * sizeof(Op)> op_queue_storage_{};
  ControlState volume_{};
  ControlState mute_{};
  std::atomic<uint8_t> speaker_volume_{100};
  std::atomic<bool> speaker_muted_{false};
  // A CONTROLS op is queued; the pending flags carry the latest values, so one is enough.
  std::atomic<bool> controls_queued_{false};
  // Set while the worker runs a driver call; platform reads and writes counted in driver_users_ back off.
  std::atomic<bool> driver_busy_{false};
  std::atomic<uint8_t> driver_users_{0};
  bool uac_installed_{false};
};

}  // namespace esphome::usb_audio

#endif
