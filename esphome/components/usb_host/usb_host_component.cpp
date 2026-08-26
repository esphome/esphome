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
#if defined(USE_ESP32_VARIANT_ESP32P4) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  if (this->dual_host_) {
    // BIT0 = FS peripheral, BIT1 = HS peripheral -- both simultaneously.
    // Requires espressif/usb >= 1.4.0 (peripheral_map field added in that release, IDF 6.0+).
    config.peripheral_map = BIT(0) | BIT(1);
    ESP_LOGI(TAG, "USB dual-host enabled (HS + FS)");
  }
#endif
  if (usb_host_install(&config) != ESP_OK) {
    this->status_set_error(LOG_STR("usb_host_install failed"));
    this->mark_failed();
    return;
  }
  usb_host_ref() = this;
}

void USBHost::loop() {
  int err;
  uint32_t event_flags;
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

bool USBHost::do_set_interface(usb_host_client_handle_t client_handle, usb_device_handle_t device_handle,
                               uint8_t interface_num, uint8_t alt_setting) {
  usb_transfer_t *xfer = nullptr;
  // SET_INTERFACE has no data phase so SETUP_PACKET_SIZE (8 bytes) is sufficient.
  esp_err_t err = usb_host_transfer_alloc(SETUP_PACKET_SIZE, 0, &xfer);
  if (err != ESP_OK || xfer == nullptr) {
    ESP_LOGE(TAG, "set_interface: alloc failed: %s", esp_err_to_name(err));
    return false;
  }

  SemaphoreHandle_t sem = xSemaphoreCreateBinary();
  if (sem == nullptr) {
    usb_host_transfer_free(xfer);
    ESP_LOGE(TAG, "set_interface: semaphore alloc failed");
    return false;
  }

  static constexpr uint8_t REQ_TYPE = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_INTERFACE;
  static constexpr uint8_t B_REQUEST_SET_INTERFACE = 11;

  auto *setup = reinterpret_cast<uint8_t *>(xfer->data_buffer);
  setup[0] = REQ_TYPE;
  setup[1] = B_REQUEST_SET_INTERFACE;
  setup[2] = alt_setting & 0xFF;
  setup[3] = 0;
  setup[4] = interface_num & 0xFF;
  setup[5] = 0;
  setup[6] = 0;
  setup[7] = 0;

  xfer->device_handle = device_handle;
  xfer->bEndpointAddress = 0;
  xfer->num_bytes = static_cast<int>(SETUP_PACKET_SIZE);
  xfer->timeout_ms = 5000;
  xfer->context = sem;
  xfer->callback = [](usb_transfer_t *t) { xSemaphoreGive(static_cast<SemaphoreHandle_t>(t->context)); };

  err = usb_host_transfer_submit_control(client_handle, xfer);
  bool ok = false;
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "set_interface: submit failed: %s", esp_err_to_name(err));
  } else {
    ok = xSemaphoreTake(sem, pdMS_TO_TICKS(6000)) == pdTRUE;
    if (!ok) {
      // ESP-IDF owns the transfer until the callback fires (guaranteed on disconnect too),
      // so do not free xfer/sem here -- the callback will run and give the semaphore.
      ESP_LOGE(TAG, "set_interface: wait timeout");
      return false;
    }
    if (xfer->status != USB_TRANSFER_STATUS_COMPLETED) {
      ESP_LOGE(TAG, "set_interface: transfer status %d", xfer->status);
      ok = false;
    }
  }

  vSemaphoreDelete(sem);
  usb_host_transfer_free(xfer);
  return ok;
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

  // Capture handles now -- they may be cleared by disconnect() before the defer runs.
  usb_host_client_handle_t client_handle = client->handle_;
  usb_device_handle_t device_handle = client->device_handle_;

  auto finish_urb = [xfer, stream, client_handle, device_handle]() {
    usb_host_transfer_free(xfer);
    if (stream->pending_urbs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      // Last URB gone. If nobody asked for a close, the stream stopped by itself and is now
      // silently dead: flag it so the owner can tear it down instead of writing into a
      // buffer that is never drained again.
      if (stream->streaming) {
        stream->streaming = false;
        stream->died.store(true, std::memory_order_release);
      }
      get_usb_host()->defer([stream, client_handle, device_handle]() {
        stream->xfers.reset();
        stream->ctxs.reset();
        if (stream->owns_interface) {
          if (stream->alt_setting != 0)
            get_usb_host()->do_set_interface(client_handle, device_handle, stream->interface_num, 0);
          get_usb_host()->do_release_interface(client_handle, device_handle, stream->interface_num);
        }
        ESP_LOGD(TAG, "stream_close deferred: ep=0x%02X", stream->ep_addr);
      });
    }
  };

  if (xfer->status == USB_TRANSFER_STATUS_NO_DEVICE || xfer->status == USB_TRANSFER_STATUS_CANCELED) {
    finish_urb();
    return;
  }

  if (stream->streaming) {
    const uint8_t *payload = xfer->data_buffer;
    uint32_t total_bytes = 0;
    for (int i = 0; i < xfer->num_isoc_packets; i++) {
      usb_isoc_packet_desc_t *desc = &xfer->isoc_packet_desc[i];
      const bool error = (desc->status != USB_TRANSFER_STATUS_COMPLETED && desc->status != USB_TRANSFER_STATUS_SKIPPED);
      // OUT: the sample clock decides how many bytes this packet may carry and on_isoc_packet
      // fills exactly that many. IN: report what was actually received, and keep asking for a
      // full mps.
      //
      // The payload stride is the packet size, not mps: the host controller lays the packets
      // out back to back, advancing by isoc_packet_desc[].num_bytes (see _buffer_fill_isoc()
      // in hcd_dwc.c), so a short packet moves every following packet down by that shortfall.
      // Writing at an mps stride only matches while every packet happens to be exactly mps.
      const uint32_t fill = stream->is_output ? isoc_next_packet_size(stream) : stream->mps;
      client->on_isoc_packet(xfer->bEndpointAddress, payload, stream->is_output ? fill : desc->actual_num_bytes,
                             error);
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

bool USBHost::stream_open(IsocStream &stream, USBClient *cb, usb_host_client_handle_t client_handle,
                          usb_device_handle_t device_handle) {
  if (stream.ep_addr == 0 || stream.mps == 0 || stream.num_urbs == 0 || stream.packets_per_urb == 0) {
    ESP_LOGE(TAG, "stream_open: invalid parameters");
    return false;
  }

  if (stream.owns_interface) {
    if (!this->do_claim_interface(client_handle, device_handle, stream.interface_num, stream.alt_setting))
      return false;

    if (stream.alt_setting != 0) {
      if (!this->do_set_interface(client_handle, device_handle, stream.interface_num, stream.alt_setting)) {
        this->do_release_interface(client_handle, device_handle, stream.interface_num);
        return false;
      }
    }
  }

  stream.xfers = std::make_unique<usb_transfer_t *[]>(stream.num_urbs);
  stream.ctxs = std::make_unique<IsocCbCtx[]>(stream.num_urbs);
  stream.pending_urbs.store(stream.num_urbs, std::memory_order_relaxed);

  stream.died.store(false, std::memory_order_relaxed);
  stream.streaming = true;
  for (uint8_t i = 0; i < stream.num_urbs; i++) {
    stream.ctxs[i].client = cb;
    stream.ctxs[i].stream = &stream;
    stream.xfers[i] = this->do_isoc_alloc(stream.ep_addr, device_handle, stream.mps, stream.packets_per_urb,
                                          USBHost::isoc_cb, &stream.ctxs[i]);
    if (stream.xfers[i] == nullptr) {
      ESP_LOGE(TAG, "stream_open: URB %u alloc failed", i);
      stream.streaming = false;
      for (uint8_t j = 0; j < i; j++)
        this->do_isoc_free(stream.xfers[j]);
      stream.ctxs.reset();
      stream.xfers.reset();
      this->do_set_interface(client_handle, device_handle, stream.interface_num, 0);
      this->do_release_interface(client_handle, device_handle, stream.interface_num);
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
      // Free unsubmitted URBs directly; already-submitted ones (0..i-1) will
      // drain through isoc_cb and free themselves. Adjust pending_urbs to
      // reflect only the submitted count so the last callback triggers cleanup.
      for (uint8_t j = i; j < stream.num_urbs; j++) {
        usb_host_transfer_free(stream.xfers[j]);
        stream.xfers[j] = nullptr;
      }
      stream.pending_urbs.store(i, std::memory_order_release);
      stream.streaming = false;
      if (i == 0) {
        // Nothing was submitted -- clean up synchronously right now.
        stream.xfers.reset();
        stream.ctxs.reset();
        this->do_set_interface(client_handle, device_handle, stream.interface_num, 0);
        this->do_release_interface(client_handle, device_handle, stream.interface_num);
      }
      return false;
    }
  }

  ESP_LOGD(TAG, "stream_open: ep=0x%02X mps=%u urbs=%u pkts/urb=%u", stream.ep_addr, stream.mps, stream.num_urbs,
           stream.packets_per_urb);
  return true;
}

void USBHost::stream_close(IsocStream &stream, usb_host_client_handle_t client_handle,
                           usb_device_handle_t device_handle) {
  if (!stream.streaming && !stream.xfers)
    return;

  // Signal callbacks to stop resubmitting. In-flight URBs will return, free
  // themselves, and the last one defers alt-setting reset, interface release,
  // and buffer cleanup to the main loop via get_usb_host()->defer() -- no blocking needed.
  stream.streaming = false;
}

#endif  // USE_USB_ISOC_TRANSFERS

}  // namespace esphome::usb_host
#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
