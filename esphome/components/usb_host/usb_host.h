#pragma once

// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include <memory>
#include <vector>
#include "usb/usb_host.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esphome/core/lock_free_queue.h"
#include "esphome/core/event_pool.h"
#include <atomic>

namespace esphome::usb_host {

// THREADING MODEL:
// This component uses a dedicated USB task for event processing to prevent data loss.
// - USB Task (high priority): Handles USB events, executes transfer callbacks, releases transfer slots
// - Main Loop Task: Initiates transfers, processes device connect/disconnect events
//
// Thread-safe communication:
// - Lock-free queues for USB task -> main loop events (SPSC pattern)
// - Lock-free TransferRequest pool using atomic bitmask (MCMP pattern)
//
// Transfer submission engine (USBHost) is stateless w.r.t. client memory -- the
// Linux URB model: client owns the pool, fills a slot, hands it to USBHost.
//
// TransferRequest pool access pattern:
// - get_trq_() [allocate]: called from BOTH the USB task and the main loop
//   * USB task: via input callbacks that restart their transfer immediately
//   * Main loop: for output transfers and flow-controlled input restarts
// - release_trq() [deallocate]: called from BOTH the USB task and the main loop
//   * USB task: right after a transfer callback completes (this is what keeps the
//     pool from running dry under load)
//   * Main loop: when a submission fails
//
// The dual-threaded allocation/deallocation is intentional rather than an oversight:
// - the USB task can restart an input transfer and give the slot back without a
//   context switch to the main loop
// - the main loop keeps control of backpressure by deciding when to restart after
//   it has consumed the data
// The atomic bitmask makes that safe without taking a mutex on either side.

static const char *const TAG = "usb_host";

// Forward declarations
struct TransferRequest;
class USBClient;
class USBHost;

// constants for setup packet type
static constexpr uint8_t USB_RECIP_DEVICE = 0;
static constexpr uint8_t USB_RECIP_INTERFACE = 1;
static constexpr uint8_t USB_RECIP_ENDPOINT = 2;
static constexpr uint8_t USB_TYPE_STANDARD = 0 << 5;
static constexpr uint8_t USB_TYPE_CLASS = 1 << 5;
static constexpr uint8_t USB_TYPE_VENDOR = 2 << 5;
static constexpr uint8_t USB_DIR_MASK = 1 << 7;
static constexpr uint8_t USB_DIR_IN = 1 << 7;
static constexpr uint8_t USB_DIR_OUT = 0;
static constexpr size_t SETUP_PACKET_SIZE = 8;

static constexpr size_t MAX_REQUESTS = USB_HOST_MAX_REQUESTS;
static_assert(MAX_REQUESTS >= 1 && MAX_REQUESTS <= 32, "MAX_REQUESTS must be between 1 and 32");

// Bitmask type tracking which TransferRequest slots are in use. It needs at least as many
// bits as MAX_REQUESTS, hence uint16_t up to 16 requests and uint32_t for 17-32. This is
// tied to the static_assert above: raising MAX_REQUESTS beyond 32 means changing both.
using trq_bitmask_t = std::conditional<(MAX_REQUESTS <= 16), uint16_t, uint32_t>::type;
static constexpr trq_bitmask_t ALL_REQUESTS_IN_USE = MAX_REQUESTS == 32 ? ~0 : (1 << MAX_REQUESTS) - 1;

static constexpr size_t USB_MAX_PACKET_SIZE = USB_HOST_MAX_PACKET_SIZE;
static constexpr size_t USB_EVENT_QUEUE_SIZE = 32;
static constexpr size_t USB_TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t USB_TASK_PRIORITY = 5;

// Transfer status reported to callback
struct TransferStatus {
  uint8_t *data;
  size_t data_len;
  void *user_data;
  uint16_t error_code;
  uint8_t endpoint;
  bool success;
};

using transfer_cb_t = std::function<void(const TransferStatus &)>;

// TransferRequest -- our URB equivalent.
// Client owns the pool, fills a slot, passes it to USBHost::submit_*().
// USBHost never allocates or frees these.
struct TransferRequest {
  usb_transfer_t *transfer;
  transfer_cb_t callback;
  TransferStatus status;
  USBClient *client;
};

enum EventType : uint8_t {
  EVENT_DEVICE_NEW,
  EVENT_DEVICE_GONE,
};

struct UsbEvent {
  EventType type;
  union {
    struct {
      uint8_t address;
    } device_new;
    struct {
      usb_device_handle_t handle;
    } device_gone;
  } data;
  void release() {}
};

enum ClientState {
  USB_CLIENT_INIT = 0,
  USB_CLIENT_OPEN,
  USB_CLIENT_CLOSE,
  USB_CLIENT_GET_DESC,
  USB_CLIENT_GET_INFO,
  USB_CLIENT_CONNECTED,
};

// -----------------------------------------------------------------------------
// Isochronous stream types -- compiled only when USE_USB_ISOC_TRANSFERS is set, which
// usb_host.require_isoc_transfers() does. That helper requests control transfers too,
// so the #error below only fires for a build that defines the macros by hand.
// -----------------------------------------------------------------------------
#if defined(USE_USB_ISOC_TRANSFERS) && !defined(USE_USB_CONTROL_TRANSFERS)
#error "USE_USB_ISOC_TRANSFERS requires USE_USB_CONTROL_TRANSFERS (alt-setting selection)"
#endif

#ifdef USE_USB_ISOC_TRANSFERS

// Where a stream is in its open sequence. Selecting the alt-setting is a control transfer
// and therefore asynchronous, so a stream is not usable the moment stream_open() returns:
// it is usable when the state reaches RUNNING, which USBClient::on_stream_open() reports.
enum class IsocOpenState : uint8_t {
  CLOSED,         // nothing claimed, no URBs, safe to open
  SELECTING_ALT,  // SET_INTERFACE submitted, waiting for the device to answer
  ABORTING,       // closed while SELECTING_ALT; its completion still has to release
  RUNNING,        // URBs submitted
};

struct IsocCbCtx {
  USBClient *client;  // vtable dispatch target for on_isoc_packet()
  struct IsocStream *stream;
};

struct IsocStream {
  std::unique_ptr<usb_transfer_t *[]> xfers {};
  std::unique_ptr<IsocCbCtx[]> ctxs{};
  std::atomic<uint8_t> pending_urbs{0};
  uint8_t num_urbs{0};
  uint8_t ep_addr{0};
  uint16_t mps{0};
  uint8_t packets_per_urb{0};
  uint8_t interface_num{0};
  uint8_t alt_setting{0};
  // Written by the main loop (stream_open/stream_close), read and written by the USB task
  // (isoc_cb). It carries the whole stop protocol, so it is atomic like every other field
  // shared between those two.
  std::atomic<bool> streaming{false};
  std::atomic<IsocOpenState> open_state{IsocOpenState::CLOSED};
  // Set from the USB task when the last URB retires while streaming was still requested,
  // i.e. the stream stopped on its own (device gone, or the periodic scheduler rejected
  // every resubmission). Nothing is transferred any more, but no close() was asked for, so
  // the owner has to notice and tear the stream down.
  std::atomic<bool> died{false};
  // UAC OUT pacing (isochronous sample clock). For an output stream each packet carries a
  // sample-rate-derived byte count instead of a full mps: sending mps-sized packets is ~2x
  // real time, overruns the device and, on the HS periodic scheduler (ESP32-P4), fails
  // resubmission. Mirrors the packet-size accumulator in Espressif's usb_host_uac.
  bool is_output{false};
  uint32_t packet_size{0};       // floor bytes per service interval (frames_floor * frame_size)
  uint32_t packet_size_frac{0};  // sample_rate % frac_div (remainder in frames per second)
  uint32_t frac_div{1000};       // service intervals per second (FS frame 1000, HS uframe 8000)
  uint32_t frac_accum{0};        // running fractional accumulator (0..frac_div-1)
  uint32_t frame_size{0};        // bytes per audio frame (channels * subframe_size)
  // Asynchronous OUT feedback: device-reported rate (samples per (micro)frame, Q10.14 at
  // full speed / Q16.16 at high speed). 0 = no feedback yet, use the nominal pacing above.
  std::atomic<uint32_t> fb_value{0};
  uint8_t fb_shift{14};  // fractional bits of fb_value (14 FS, 16 HS)
  uint32_t fb_accum{0};  // feedback fractional accumulator
  // A feedback stream shares its AS interface with the data stream, so it must not claim
  // or release that interface itself.
  bool owns_interface{true};

  // True once the URBs are in flight. Between stream_open() and this, the device is still
  // being switched to the alt-setting and the endpoint does not exist yet.
  bool is_running() const { return this->open_state.load(std::memory_order_acquire) == IsocOpenState::RUNNING; }
  // True when nothing is claimed and no URB is left, i.e. the stream may be opened again.
  bool is_closed() const {
    return this->open_state.load(std::memory_order_acquire) == IsocOpenState::CLOSED &&
           this->pending_urbs.load(std::memory_order_acquire) == 0;
  }
};

#endif  // USE_USB_ISOC_TRANSFERS

// -----------------------------------------------------------------------------
// USBClient -- device state machine + thin forwarding layer to USBHost.
// -----------------------------------------------------------------------------
class USBClient : public Component {
  friend class USBHost;

 public:
  USBClient(uint16_t vid, uint16_t pid) : trq_in_use_(0), vid_(vid), pid_(pid) {}
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::IO; }
  void on_opened(uint8_t addr);
  virtual void on_removed(usb_device_handle_t handle);
  void dump_config() override;
  // THREAD CONTEXT: both the USB task and the main loop
  // - USB task: immediately after a transfer callback completes
  // - Main loop: when a transfer submission failed
  void release_trq(TransferRequest *trq);
  trq_bitmask_t get_trq_in_use() const { return trq_in_use_; }

  void set_required_interface_class(uint8_t cls) {
    this->match_any_interface_class_ = false;
    this->required_interface_class_ = cls;
  }

  // Lock-free event queue and pool -- public for static callbacks
  LockFreeQueue<UsbEvent, USB_EVENT_QUEUE_SIZE> event_queue;
  // The pool is one entry smaller than the queue because LockFreeQueue<T, N> is a ring
  // buffer holding N-1 elements. Sizing it that way makes allocate() return nullptr before
  // push() can fail, so a pool slot is never leaked on a full queue. The "- 1" is that
  // guarantee, not an off-by-one.
  EventPool<UsbEvent, USB_EVENT_QUEUE_SIZE - 1> event_pool;

  // -- Bulk / interrupt transfers ----------------------------------------------
  // Compiled only when a component calls usb_host.require_bulk_transfers() from its
  // to_code(); without that, these members do not exist and calls fail to compile.
#ifdef USE_USB_BULK_TRANSFERS
  // THREAD CONTEXT: both the USB task and the main loop
  // - USB task: an input callback restarting its own transfer for immediate reception
  // - Main loop: initial setup, and flow-controlled restarts after data was consumed
  bool transfer_in(uint8_t ep_address, const transfer_cb_t &callback, uint16_t length);
  // THREAD CONTEXT: both the USB task and the main loop
  // - USB task: an output callback restarting output directly, without a defer
  // - Main loop: the initial output trigger from write_array() and loop()
  bool transfer_out(uint8_t ep_address, const transfer_cb_t &callback, const uint8_t *data, uint16_t length);
#endif

  // -- Control transfers -------------------------------------------------------
  // Compiled only when a component calls usb_host.require_control_transfers() from its
  // to_code(); without that, these members do not exist and calls fail to compile.
#ifdef USE_USB_CONTROL_TRANSFERS
  // w_length is what goes into the setup packet's wLength field, i.e. the number of bytes
  // the request itself is defined to carry. It defaults to the size of data, which is what
  // a request whose data stage is exactly its buffer wants.
  //
  // They are separate because the host controller sizes the data stage from the buffer and
  // not from wLength (see the comment in the IDF's _buffer_fill_ctrl), and an IN data stage
  // needs a buffer of whole endpoint packets. A control read therefore rounds its buffer up
  // and passes the length the class defines here, so what goes on the wire stays the
  // request the device expects. Negative means "take it from data".
  bool control_transfer(uint8_t type, uint8_t request, uint16_t value, uint16_t index, const transfer_cb_t &callback,
                        const std::vector<uint8_t> &data = {}, int32_t w_length = -1);
#endif

  // -- Interface claim / release / alt-setting ---------------------------------
  bool claim_interface(uint8_t interface_num, uint8_t alt_setting = 0);
  bool release_interface(uint8_t interface_num);

#ifdef USE_USB_CONTROL_TRANSFERS
  bool set_interface(uint8_t interface_num, uint8_t alt_setting, const transfer_cb_t &callback);
#endif

  // -- Isochronous support -----------------------------------------------------
  // Compiled only when a component calls usb_host.require_isoc_transfers() from its
  // to_code(); without that, these members do not exist and calls fail to compile.
#ifdef USE_USB_ISOC_TRANSFERS
  usb_transfer_t *isoc_alloc(uint8_t ep_addr, uint16_t mps, uint8_t num_packets, usb_transfer_cb_t callback,
                             void *context);
  bool isoc_submit(usb_transfer_t *xfer);
  void isoc_free(usb_transfer_t *xfer);

  // Starts the open sequence. A true return only means it started: an owning stream has to
  // select its alt-setting first, which is a control transfer. on_stream_open() reports the
  // outcome, and is_running() tells whether the stream is carrying data.
  //
  // Callback contract: once the open is accepted, on_stream_open() is called exactly once,
  // whether the sequence succeeds or fails, so a state machine may key off it alone. The
  // two precondition checks (invalid parameters, stream not closed yet) reject the call
  // before it is accepted and report through the return value only -- they say nothing
  // about the stream that is already there.
  bool stream_open(IsocStream &stream, USBClient *cb);
  void stream_close(IsocStream &stream);

  // Override in subclass to consume one received isochronous packet (IN endpoints).
  // Called from USB-task context -- must be fast and non-blocking.
  virtual void on_isoc_packet(uint8_t ep_addr, const uint8_t *data, size_t len, bool error) {}
  // Override in subclass to produce one isochronous packet (OUT endpoints). max_len is what
  // the sample clock allows this packet to carry; return how many bytes were written. The
  // rest of the packet is zero filled, so a short return is silence rather than stale audio.
  // Called from USB-task context -- must be fast and non-blocking.
  virtual size_t on_isoc_fill(uint8_t ep_addr, uint8_t *data, size_t max_len) { return 0; }
  // Called on the main loop once the open sequence finished, successfully or not.
  virtual void on_stream_open(IsocStream &stream, bool ok) {}
#endif

 protected:
  bool process_usb_events_();
  void handle_open_state_();
  TransferRequest *get_trq_();
  virtual void disconnect();
  virtual void on_connected() {}
  virtual void on_disconnected() { this->trq_in_use_.store(0); }

  static void usb_task_fn(void *arg);
  [[noreturn]] void usb_task_loop_() const;
  static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg);

  TransferRequest requests_[MAX_REQUESTS]{};
  TaskHandle_t usb_task_handle_{nullptr};
  usb_host_client_handle_t handle_{};
  usb_device_handle_t device_handle_{};
  const usb_device_desc_t *device_desc_{nullptr};
  const usb_config_desc_t *config_desc_{nullptr};
  int device_addr_{-1};
  int state_{USB_CLIENT_INIT};
  // Lock-free pool management, no dynamic allocation: bit i set means requests_[i] is in
  // use. Both threads allocate and deallocate, hence the atomic (see the header comment).
  std::atomic<trq_bitmask_t> trq_in_use_;
  uint16_t vid_{};
  uint16_t pid_{};
  bool match_any_interface_class_{true};
  uint8_t required_interface_class_{0};
  // Resolved once in setup(); the forwarders below would otherwise dereference the global
  // accessor unchecked on every call, including on the RX hot path.
  USBHost *host_{nullptr};

  const usb_device_desc_t *get_device_desc_() const { return this->device_desc_; }
  const usb_config_desc_t *get_config_desc_() const { return this->config_desc_; }
};
// -----------------------------------------------------------------------------
// USBHost -- USB host stack + stateless transfer submission engine.
//
// Acts as the Linux host controller driver: owns usb_host_install(), the lib
// event loop, and all ESP-IDF transfer submission calls.  Never touches client
// memory -- clients own their TransferRequest pools and hand filled slots here.
// -----------------------------------------------------------------------------
class USBHost final : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS; }
  void loop() override;
  void setup() override;

  // -- Submission engine (called by USBClient thin forwarders) ----------------

  // Bulk / interrupt IN and OUT -- always compiled if any client uses them
  bool submit_transfer(TransferRequest *trq);

  // Control transfer submission -- guarded
#ifdef USE_USB_CONTROL_TRANSFERS
  bool submit_control(usb_host_client_handle_t client_handle, TransferRequest *trq);
#endif

  // Interface claim / release -- always needed
  bool do_claim_interface(usb_host_client_handle_t client_handle, usb_device_handle_t device_handle,
                          uint8_t interface_num, uint8_t alt_setting);
  bool do_release_interface(usb_host_client_handle_t client_handle, usb_device_handle_t device_handle,
                            uint8_t interface_num);

  // -- Isochronous -------------------------------------------------------------
#ifdef USE_USB_ISOC_TRANSFERS
  usb_transfer_t *do_isoc_alloc(uint8_t ep_addr, usb_device_handle_t device_handle, uint16_t mps, uint8_t num_packets,
                                usb_transfer_cb_t callback, void *context);
  bool do_isoc_submit(usb_transfer_t *xfer);
  void do_isoc_free(usb_transfer_t *xfer);

  bool stream_open(IsocStream &stream, USBClient *cb, usb_host_client_handle_t client_handle,
                   usb_device_handle_t device_handle);
  void stream_close(IsocStream &stream);

  // Second half of the open sequence: allocate the URBs, seed an OUT stream with silence
  // and submit. Split out so it can run either straight from stream_open() or from the
  // SET_INTERFACE completion.
  bool stream_start_urbs(IsocStream &stream, USBClient *cb, usb_device_handle_t device_handle);
  // Give back whatever the stream holds of its interface. Does nothing for a stream that
  // shares its interface with another one, and skips the alt-setting reset when there is
  // nothing to reset.
  void stream_release(IsocStream &stream, USBClient *cb, usb_host_client_handle_t client_handle,
                      usb_device_handle_t device_handle);

  // Static trampoline stored as xfer->callback for every ISOC URB.
  // Iterates isoc_packet_desc[], calls client->on_isoc_packet() per packet, resubmits.
  static void isoc_cb(usb_transfer_t *xfer);
#endif

 protected:
  std::vector<USBClient *> clients_{};
};

// Returns the global USBHost singleton, set during USBHost::setup().
USBHost *get_usb_host();

}  // namespace esphome::usb_host

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
