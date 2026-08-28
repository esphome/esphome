// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "usb_host.h"
#include <cinttypes>
#include "esphome/core/log.h"

namespace esphome::usb_host {

static USBHost *&usb_host_ref() {
  static USBHost *instance = nullptr;
  return instance;
}

USBHost *get_usb_host() { return usb_host_ref(); }

void USBHost::setup() {
  usb_host_config_t config{};
  if (usb_host_install(&config) != ESP_OK) {
    this->status_set_error(LOG_STR("usb_host_install failed"));
    this->mark_failed();
    return;
  }
  usb_host_ref() = this;
}

void USBHost::loop() {
  int err;
  // usb_host_lib_handle_events() does not write event_flags on ESP_ERR_TIMEOUT, which is
  // the normal case for a zero timeout, so reading it uninitialised below is UB.
  uint32_t event_flags = 0;
  err = usb_host_lib_handle_events(0, &event_flags);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGD(TAG, "lib_handle_events failed: %s", esp_err_to_name(err));
  }
  if (event_flags != 0) {
    ESP_LOGD(TAG, "Event flags %" PRIu32 "X", event_flags);
  }
}

// -- Submission engine ---------------------------------------------------------

bool USBHost::submit_transfer(TransferRequest *trq) {
  esp_err_t err = usb_host_transfer_submit(trq->transfer);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "submit_transfer failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

// -- Interface claim / release -------------------------------------------------

bool USBHost::do_claim_interface(usb_host_client_handle_t client_handle, usb_device_handle_t device_handle,
                                 uint8_t interface_num, uint8_t alt_setting) {
  esp_err_t err = usb_host_interface_claim(client_handle, device_handle, interface_num, alt_setting);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "claim_interface %u alt %u failed: %s", interface_num, alt_setting, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool USBHost::do_release_interface(usb_host_client_handle_t client_handle, usb_device_handle_t device_handle,
                                   uint8_t interface_num) {
  esp_err_t err = usb_host_interface_release(client_handle, device_handle, interface_num);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "release_interface %u failed: %s", interface_num, esp_err_to_name(err));
    return false;
  }
  return true;
}

// -- Control transfer submission -----------------------------------------------
#ifdef USE_USB_CONTROL_TRANSFERS

bool USBHost::submit_control(usb_host_client_handle_t client_handle, TransferRequest *trq) {
  esp_err_t err = usb_host_transfer_submit_control(client_handle, trq->transfer);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "submit_control failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

#endif  // USE_USB_CONTROL_TRANSFERS

// -- Isochronous ---------------------------------------------------------------
#ifdef USE_USB_ISOC_TRANSFERS

usb_transfer_t *USBHost::do_isoc_alloc(uint8_t ep_addr, usb_device_handle_t device_handle, uint16_t mps,
                                       uint8_t num_packets, usb_transfer_cb_t callback, void *context) {
  if (mps == 0 || num_packets == 0) {
    ESP_LOGE(TAG, "isoc_alloc: invalid mps=%u or num_packets=%u", mps, num_packets);
    return nullptr;
  }
  size_t data_size = static_cast<size_t>(mps) * num_packets;
  usb_transfer_t *xfer = nullptr;
  esp_err_t err = usb_host_transfer_alloc(data_size, num_packets, &xfer);
  if (err != ESP_OK || xfer == nullptr) {
    ESP_LOGE(TAG, "isoc_alloc: alloc failed: %s", esp_err_to_name(err));
    return nullptr;
  }
  xfer->device_handle = device_handle;
  xfer->bEndpointAddress = ep_addr;
  xfer->num_bytes = static_cast<int>(data_size);
  xfer->timeout_ms = 0;
  xfer->callback = callback;
  xfer->context = context;
  for (int i = 0; i < num_packets; i++) {
    xfer->isoc_packet_desc[i].num_bytes = mps;
  }
  return xfer;
}

bool USBHost::do_isoc_submit(usb_transfer_t *xfer) {
  if (xfer == nullptr)
    return false;
  esp_err_t err = usb_host_transfer_submit(xfer);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "isoc_submit failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

void USBHost::do_isoc_free(usb_transfer_t *xfer) {
  if (xfer != nullptr)
    usb_host_transfer_free(xfer);
}

// Next OUT packet size for the sample-clock pacing: the floor plus one audio frame whenever
// the fractional accumulator wraps (keeps non-integer frames-per-service-interval rates such
// as 44.1 kHz frame-aligned). Clamped to mps as a safety net. Advances stream state.
static uint32_t isoc_next_packet_size(IsocStream *stream) {
  uint32_t psize;
  const uint32_t fb = stream->fb_value.load(std::memory_order_relaxed);
  if (fb != 0) {
    // Asynchronous endpoint: the device-reported feedback (samples per service interval in
    // Q10.14 / Q16.16) drives the rate instead of the nominal sample_rate.
    const uint32_t one = 1u << stream->fb_shift;
    psize = (fb >> stream->fb_shift) * stream->frame_size;
    stream->fb_accum += fb & (one - 1);
    if (stream->fb_accum >= one) {
      stream->fb_accum -= one;
      psize += stream->frame_size;
    }
  } else {
    psize = stream->packet_size;
    stream->frac_accum += stream->packet_size_frac;
    if (stream->frac_div != 0 && stream->frac_accum >= stream->frac_div) {
      stream->frac_accum -= stream->frac_div;
      psize += stream->frame_size;
    }
  }
  if (psize == 0 || psize > stream->mps)
    psize = stream->mps;
  return psize;
}

// CALLBACK CONTEXT: USB task
void USBHost::isoc_cb(usb_transfer_t *xfer) {
  auto *ctx = static_cast<IsocCbCtx *>(xfer->context);
  USBClient *client = ctx->client;
  IsocStream *stream = ctx->stream;

  auto finish_urb = [xfer, stream, client]() {
    usb_host_transfer_free(xfer);
    if (stream->pending_urbs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      // Last URB gone. If nobody asked for a close, the stream stopped by itself and is now
      // silently dead: flag it so the owner can tear it down instead of writing into a
      // buffer that is never drained again.
      if (stream->streaming.exchange(false, std::memory_order_acq_rel))
        stream->died.store(true, std::memory_order_release);
      get_usb_host()->defer([stream, client] {
        stream->xfers.reset();
        stream->ctxs.reset();
        // Read the handles here rather than snapshotting them in the USB-task callback.
        // This lambda and disconnect() both run on the main loop, so either the device is
        // still open and the handles are valid, or disconnect() already closed it and
        // device_handle_ is null. Snapshotting would turn that null into a stale handle:
        // releasing an interface on a device the IDF has freed, or -- if the defer wins the
        // race -- leaving the interface claimed so usb_host_device_close() refuses and the
        // device object leaks for the life of the process.
        if (client->device_handle_ != nullptr) {
          get_usb_host()->stream_release(*stream, client, client->handle_, client->device_handle_);
        }
        stream->open_state.store(IsocOpenState::CLOSED, std::memory_order_release);
        ESP_LOGD(TAG, "stream_close deferred: ep=0x%02X", stream->ep_addr);
      });
    }
  };

  if (xfer->status == USB_TRANSFER_STATUS_NO_DEVICE || xfer->status == USB_TRANSFER_STATUS_CANCELED) {
    finish_urb();
    return;
  }

  if (stream->streaming.load(std::memory_order_acquire)) {
    uint8_t *payload = xfer->data_buffer;
    uint32_t total_bytes = 0;
    for (int i = 0; i < xfer->num_isoc_packets; i++) {
      usb_isoc_packet_desc_t *desc = &xfer->isoc_packet_desc[i];
      const bool error = (desc->status != USB_TRANSFER_STATUS_COMPLETED && desc->status != USB_TRANSFER_STATUS_SKIPPED);
      // OUT: the sample clock decides how many bytes this packet may carry and on_isoc_fill()
      // writes into the URB. IN: report what was actually received, and keep asking for a
      // full mps.
      //
      // The payload stride is the packet size, not mps: the host controller lays the packets
      // out back to back, advancing by isoc_packet_desc[].num_bytes (see _buffer_fill_isoc()
      // in hcd_dwc.c), so a short packet moves every following packet down by that shortfall.
      // Writing at an mps stride only matches while every packet happens to be exactly mps.
      const uint32_t fill = stream->is_output ? isoc_next_packet_size(stream) : stream->mps;
      if (stream->is_output) {
        const size_t written = client->on_isoc_fill(xfer->bEndpointAddress, payload, fill);
        // Whatever the client did not produce is silence, not the previous packet again.
        if (written < fill)
          memset(payload + written, 0, fill - written);
      } else {
        client->on_isoc_packet(xfer->bEndpointAddress, payload, desc->actual_num_bytes, error);
      }
      desc->num_bytes = fill;
      payload += fill;
      total_bytes += fill;
    }
    // Isochronous transfers are described by the packet list, but keep the transfer length
    // consistent with it rather than leaving the allocation size behind.
    xfer->num_bytes = static_cast<int>(total_bytes);
    if (usb_host_transfer_submit(xfer) != ESP_OK) {
      // Resubmission failed (e.g. periodic scheduler rejected it): retire this URB instead of
      // leaving it dangling. The stream keeps running on the remaining URBs, and when the last
      // one is retired finish_urb() tears the stream down cleanly rather than stalling silently.
      ESP_LOGW(TAG, "isoc resubmit failed (ep=0x%02X), retiring URB", xfer->bEndpointAddress);
      finish_urb();
    }
  } else {
    // stream_close() set streaming=false -- free and defer cleanup when all URBs done.
    finish_urb();
  }
}

bool USBHost::stream_start_urbs(IsocStream &stream, USBClient *cb, usb_device_handle_t device_handle) {
  stream.xfers = std::make_unique<usb_transfer_t *[]>(stream.num_urbs);
  stream.ctxs = std::make_unique<IsocCbCtx[]>(stream.num_urbs);
  stream.pending_urbs.store(stream.num_urbs, std::memory_order_relaxed);
  stream.died.store(false, std::memory_order_relaxed);
  stream.streaming.store(true, std::memory_order_release);

  for (uint8_t i = 0; i < stream.num_urbs; i++) {
    stream.ctxs[i].client = cb;
    stream.ctxs[i].stream = &stream;
    stream.xfers[i] = this->do_isoc_alloc(stream.ep_addr, device_handle, stream.mps, stream.packets_per_urb,
                                          USBHost::isoc_cb, &stream.ctxs[i]);
    if (stream.xfers[i] == nullptr) {
      ESP_LOGE(TAG, "stream_open: URB %u alloc failed", i);
      stream.streaming.store(false, std::memory_order_release);
      for (uint8_t j = 0; j < i; j++)
        this->do_isoc_free(stream.xfers[j]);
      stream.pending_urbs.store(0, std::memory_order_release);
      stream.ctxs.reset();
      stream.xfers.reset();
      return false;
    }
  }

  if (stream.is_output) {
    // Seed the initial OUT URBs with silence sized to the sample clock, so the very first
    // transfers already fit the negotiated bandwidth instead of a full mps.
    for (uint8_t i = 0; i < stream.num_urbs; i++) {
      memset(stream.xfers[i]->data_buffer, 0, static_cast<size_t>(stream.mps) * stream.packets_per_urb);
      uint32_t total_bytes = 0;
      for (uint8_t j = 0; j < stream.packets_per_urb; j++) {
        const uint32_t fill = isoc_next_packet_size(&stream);
        stream.xfers[i]->isoc_packet_desc[j].num_bytes = fill;
        total_bytes += fill;
      }
      stream.xfers[i]->num_bytes = static_cast<int>(total_bytes);
    }
  }

  for (uint8_t i = 0; i < stream.num_urbs; i++) {
    if (!this->do_isoc_submit(stream.xfers[i])) {
      ESP_LOGE(TAG, "stream_open: URB %u submit failed", i);
      // URBs 0..i-1 are already in flight and free themselves through isoc_cb. Stop the
      // resubmission first, then free the ones that never went out and take them off the
      // count relatively: a blind store would overwrite a decrement an in-flight URB has
      // already made, and the count would never reach zero.
      stream.streaming.store(false, std::memory_order_release);
      for (uint8_t j = i; j < stream.num_urbs; j++) {
        usb_host_transfer_free(stream.xfers[j]);
        stream.xfers[j] = nullptr;
      }
      const uint8_t never_submitted = stream.num_urbs - i;
      if (stream.pending_urbs.fetch_sub(never_submitted, std::memory_order_acq_rel) == never_submitted) {
        // Every submitted URB had already retired, so nothing will call finish_urb() again
        // and the buffers are ours to drop right here.
        stream.xfers.reset();
        stream.ctxs.reset();
      }
      return false;
    }
  }

  ESP_LOGD(TAG, "stream_open: ep=0x%02X mps=%u urbs=%u pkts/urb=%u", stream.ep_addr, stream.mps, stream.num_urbs,
           stream.packets_per_urb);
  return true;
}

void USBHost::stream_release(IsocStream &stream, USBClient *cb, usb_host_client_handle_t client_handle,
                             usb_device_handle_t device_handle) {
  if (!stream.owns_interface)
    return;
  // Only the owner may touch the alt-setting: a feedback stream shares its interface with
  // the data stream, and resetting it here would tear that one down.
  if (stream.alt_setting != 0) {
    cb->set_interface(stream.interface_num, 0, [](const TransferStatus &status) {
      if (!status.success)
        ESP_LOGW(TAG, "alt-setting reset failed");
    });
  }
  this->do_release_interface(client_handle, device_handle, stream.interface_num);
}

bool USBHost::stream_open(IsocStream &stream, USBClient *cb, usb_host_client_handle_t client_handle,
                          usb_device_handle_t device_handle) {
  // Precondition violations report through the return value only. Firing on_stream_open()
  // here would tell the owner of an already running stream that its stream just failed.
  if (stream.ep_addr == 0 || stream.mps == 0 || stream.num_urbs == 0 || stream.packets_per_urb == 0) {
    ESP_LOGE(TAG, "stream_open: invalid parameters");
    return false;
  }
  // Opening on top of a stream that is still draining would overwrite xfers/ctxs while
  // in-flight callbacks still point into them.
  if (!stream.is_closed()) {
    ESP_LOGW(TAG, "stream_open: ep=0x%02X is not closed yet", stream.ep_addr);
    return false;
  }
  // Past this point the open has been accepted, so every exit -- here or from the
  // SET_INTERFACE completion -- calls on_stream_open() exactly once.

  if (!stream.owns_interface) {
    // Shares an already selected interface with the stream that owns it, so there is
    // nothing to claim and nothing to switch.
    const bool ok = this->stream_start_urbs(stream, cb, device_handle);
    stream.open_state.store(ok ? IsocOpenState::RUNNING : IsocOpenState::CLOSED, std::memory_order_release);
    cb->on_stream_open(stream, ok);
    return ok;
  }

  if (!this->do_claim_interface(client_handle, device_handle, stream.interface_num, stream.alt_setting)) {
    cb->on_stream_open(stream, false);
    return false;
  }

  if (stream.alt_setting == 0) {
    const bool ok = this->stream_start_urbs(stream, cb, device_handle);
    // Only give the interface back when nothing is draining: if URBs did go out, the last
    // one to retire runs the same teardown and doing it twice releases it twice.
    if (!ok && stream.pending_urbs.load(std::memory_order_acquire) == 0)
      this->stream_release(stream, cb, client_handle, device_handle);
    stream.open_state.store(ok ? IsocOpenState::RUNNING : IsocOpenState::CLOSED, std::memory_order_release);
    cb->on_stream_open(stream, ok);
    return ok;
  }

  // The endpoint only exists once the device has switched to the alt-setting, and switching
  // is a control transfer. Submit it and finish the open from its completion rather than
  // blocking the caller -- every caller here is the main loop.
  stream.open_state.store(IsocOpenState::SELECTING_ALT, std::memory_order_release);
  const bool submitted =
      cb->set_interface(stream.interface_num, stream.alt_setting, [this, &stream, cb](const TransferStatus &status) {
        // Completion runs on the USB task; hand the rest to the main loop so the whole open
        // sequence stays on the task that started it.
        const bool selected = status.success;
        this->defer([this, &stream, cb, selected]() {
          // Read the handles here rather than snapshotting them at submit time. A full
          // control round trip fits in this window, and disconnect() runs on the main loop
          // just like this lambda, so either the device is still open and the handles are
          // valid, or it is gone and device_handle_ is null. A snapshot would turn that
          // null into a stale handle and release an interface on a device the IDF freed.
          // Not const-qualified: both are pointer typedefs, so a leading const would apply
          // to the pointer rather than the pointee and trips clang-tidy misc-misplaced-const.
          usb_host_client_handle_t client_handle = cb->handle_;
          usb_device_handle_t device_handle = cb->device_handle_;
          if (device_handle == nullptr) {
            // The device went away mid-switch. It took the claimed interface with it, so
            // there is nothing to release.
            stream.open_state.store(IsocOpenState::CLOSED, std::memory_order_release);
            cb->on_stream_open(stream, false);
            return;
          }
          // The owner may have closed the stream while the device was being switched. Give
          // the interface back rather than starting URBs nobody asked for any more, and
          // only then report the stream closed: this is the completion the ABORTING state
          // was waiting for.
          if (stream.open_state.load(std::memory_order_acquire) != IsocOpenState::SELECTING_ALT) {
            this->stream_release(stream, cb, client_handle, device_handle);
            stream.open_state.store(IsocOpenState::CLOSED, std::memory_order_release);
            cb->on_stream_open(stream, false);
            return;
          }
          bool ok = selected;
          if (!ok) {
            ESP_LOGE(TAG, "stream_open: SET_INTERFACE %u alt %u failed", stream.interface_num, stream.alt_setting);
          } else {
            ok = this->stream_start_urbs(stream, cb, device_handle);
          }
          if (!ok) {
            if (stream.pending_urbs.load(std::memory_order_acquire) == 0)
              this->stream_release(stream, cb, client_handle, device_handle);
            stream.open_state.store(IsocOpenState::CLOSED, std::memory_order_release);
          } else {
            stream.open_state.store(IsocOpenState::RUNNING, std::memory_order_release);
          }
          cb->on_stream_open(stream, ok);
        });
      });
  if (!submitted) {
    ESP_LOGE(TAG, "stream_open: SET_INTERFACE %u alt %u not submitted", stream.interface_num, stream.alt_setting);
    this->stream_release(stream, cb, client_handle, device_handle);
    stream.open_state.store(IsocOpenState::CLOSED, std::memory_order_release);
    cb->on_stream_open(stream, false);
    return false;
  }
  return true;
}

void USBHost::stream_close(IsocStream &stream) {
  // An open that has not started its URBs yet is cancelled by taking it out of
  // SELECTING_ALT: the SET_INTERFACE completion then gives the interface back instead of
  // starting a stream nobody wants. There is no URB to drain, but the interface is still
  // claimed and the completion is still queued, so the stream is not closed yet -- going
  // straight to CLOSED here would make is_closed() true and let a reopen claim an
  // interface the old open never released, with the old completion then driving the new
  // open. ABORTING keeps is_closed() false until that completion has run.
  auto selecting = IsocOpenState::SELECTING_ALT;
  if (stream.open_state.compare_exchange_strong(selecting, IsocOpenState::ABORTING, std::memory_order_acq_rel))
    return;

  if (!stream.streaming.load(std::memory_order_acquire) && !stream.xfers)
    return;

  // Signal callbacks to stop resubmitting. In-flight URBs will return, free themselves, and
  // the last one defers alt-setting reset, interface release and buffer cleanup to the main
  // loop via defer() -- no blocking needed. is_closed() reports when that has happened.
  stream.streaming.store(false, std::memory_order_release);
}

#endif  // USE_USB_ISOC_TRANSFERS

}  // namespace esphome::usb_host
#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
