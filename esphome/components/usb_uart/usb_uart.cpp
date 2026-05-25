// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_uart.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <cinttypes>

namespace esphome::usb_uart {

/**
 *
 * Given a configuration, look for the required interfaces defining a CDC-ACM device
 * @param config_desc The configuration descriptor
 * @param intf_idx The index of the interface to be examined
 * @return
 */
static optional<CdcEps> get_cdc(const usb_config_desc_t *config_desc, uint8_t intf_idx) {
  int conf_offset, ep_offset;
  // look for an interface with an interrupt endpoint (notify), and one with two bulk endpoints (data in/out)
  CdcEps eps{};
  eps.bulk_interface_number = 0xFF;
  eps.interrupt_interface_number = 0xFF;
  for (;;) {
    const auto *intf_desc = usb_parse_interface_descriptor(config_desc, intf_idx++, 0, &conf_offset);
    if (!intf_desc) {
      ESP_LOGE(TAG, "usb_parse_interface_descriptor failed");
      return nullopt;
    }
    ESP_LOGD(TAG, "intf_desc: bInterfaceClass=%02X, bInterfaceSubClass=%02X, bInterfaceProtocol=%02X, bNumEndpoints=%d",
             intf_desc->bInterfaceClass, intf_desc->bInterfaceSubClass, intf_desc->bInterfaceProtocol,
             intf_desc->bNumEndpoints);
    for (uint8_t i = 0; i != intf_desc->bNumEndpoints; i++) {
      ep_offset = conf_offset;
      const auto *ep = usb_parse_endpoint_descriptor_by_index(intf_desc, i, config_desc->wTotalLength, &ep_offset);
      if (!ep) {
        ESP_LOGE(TAG, "Ran out of interfaces at %d before finding all endpoints", i);
        return nullopt;
      }
      ESP_LOGD(TAG, "ep: bEndpointAddress=%02X, bmAttributes=%02X", ep->bEndpointAddress, ep->bmAttributes);
      if (ep->bmAttributes == USB_BM_ATTRIBUTES_XFER_INT) {
        eps.notify_ep = ep;
        eps.interrupt_interface_number = intf_desc->bInterfaceNumber;
      } else if (ep->bmAttributes == USB_BM_ATTRIBUTES_XFER_BULK && ep->bEndpointAddress & usb_host::USB_DIR_IN &&
                 (eps.bulk_interface_number == 0xFF || eps.bulk_interface_number == intf_desc->bInterfaceNumber)) {
        eps.in_ep = ep;
        eps.bulk_interface_number = intf_desc->bInterfaceNumber;
      } else if (ep->bmAttributes == USB_BM_ATTRIBUTES_XFER_BULK && !(ep->bEndpointAddress & usb_host::USB_DIR_IN) &&
                 (eps.bulk_interface_number == 0xFF || eps.bulk_interface_number == intf_desc->bInterfaceNumber)) {
        eps.out_ep = ep;
        eps.bulk_interface_number = intf_desc->bInterfaceNumber;
      } else {
        ESP_LOGE(TAG, "Unexpected endpoint attributes: %02X", ep->bmAttributes);
        continue;
      }
    }
    if (eps.in_ep != nullptr && eps.out_ep != nullptr && eps.notify_ep != nullptr)
      return eps;
  }
}

std::vector<CdcEps> USBUartTypeCdcAcm::parse_descriptors(usb_device_handle_t dev_hdl) {
  const usb_config_desc_t *config_desc;
  const usb_device_desc_t *device_desc;
  int desc_offset = 0;
  std::vector<CdcEps> cdc_devs{};

  // Get required descriptors
  if (usb_host_get_device_descriptor(dev_hdl, &device_desc) != ESP_OK) {
    ESP_LOGE(TAG, "get_device_descriptor failed");
    return {};
  }
  if (usb_host_get_active_config_descriptor(dev_hdl, &config_desc) != ESP_OK) {
    ESP_LOGE(TAG, "get_active_config_descriptor failed");
    return {};
  }
  if (device_desc->bDeviceClass == USB_CLASS_COMM || device_desc->bDeviceClass == USB_CLASS_VENDOR_SPEC) {
    // single CDC-ACM device
    if (auto eps = get_cdc(config_desc, 0)) {
      ESP_LOGV(TAG, "Found CDC-ACM device");
      cdc_devs.push_back(*eps);
    }
    return cdc_devs;
  }
  if (((device_desc->bDeviceClass == USB_CLASS_MISC) && (device_desc->bDeviceSubClass == USB_SUBCLASS_COMMON) &&
       (device_desc->bDeviceProtocol == USB_DEVICE_PROTOCOL_IAD)) ||
      ((device_desc->bDeviceClass == USB_CLASS_PER_INTERFACE) && (device_desc->bDeviceSubClass == USB_SUBCLASS_NULL) &&
       (device_desc->bDeviceProtocol == USB_PROTOCOL_NULL))) {
    // This is a composite device, that uses Interface Association Descriptor
    const auto *this_desc = reinterpret_cast<const usb_standard_desc_t *>(config_desc);
    for (;;) {
      this_desc = usb_parse_next_descriptor_of_type(this_desc, config_desc->wTotalLength,
                                                    USB_B_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION, &desc_offset);
      if (!this_desc)
        break;
      const auto *iad_desc = reinterpret_cast<const usb_iad_desc_t *>(this_desc);

      if (iad_desc->bFunctionClass == USB_CLASS_COMM && iad_desc->bFunctionSubClass == USB_CDC_SUBCLASS_ACM) {
        ESP_LOGV(TAG, "Found CDC-ACM device in composite device");
        if (auto eps = get_cdc(config_desc, iad_desc->bFirstInterface))
          cdc_devs.push_back(*eps);
      }
    }
  }
  return cdc_devs;
}

void RingBuffer::push(uint8_t item) {
  if (this->get_free_space() == 0)
    return;
  this->buffer_[this->insert_pos_] = item;
  this->insert_pos_ = (this->insert_pos_ + 1) % this->buffer_size_;
}
void RingBuffer::push(const uint8_t *data, size_t len) {
  size_t free = this->get_free_space();
  if (len > free)
    len = free;
  for (size_t i = 0; i != len; i++) {
    this->buffer_[this->insert_pos_] = *data++;
    this->insert_pos_ = (this->insert_pos_ + 1) % this->buffer_size_;
  }
}

uint8_t RingBuffer::pop() {
  uint8_t item = this->buffer_[this->read_pos_];
  this->read_pos_ = (this->read_pos_ + 1) % this->buffer_size_;
  return item;
}
size_t RingBuffer::pop(uint8_t *data, size_t len) {
  len = std::min(len, this->get_available());
  for (size_t i = 0; i != len; i++) {
    *data++ = this->buffer_[this->read_pos_];
    this->read_pos_ = (this->read_pos_ + 1) % this->buffer_size_;
  }
  return len;
}
void USBUartChannel::write_array(const uint8_t *data, size_t len) {
  if (!this->initialised_.load()) {
    ESP_LOGD(TAG, "Channel not initialised - write ignored");
    return;
  }
#ifdef USE_UART_DEBUGGER
  if (this->debug_) {
    constexpr size_t BATCH = 16;
    char buf[4 + format_hex_pretty_size(BATCH)];  // ">>> " + "XX,XX,...,XX\0"
    for (size_t off = 0; off < len; off += BATCH) {
      size_t n = std::min(len - off, BATCH);
      memcpy(buf, ">>> ", 4);
      format_hex_pretty_to(buf + 4, sizeof(buf) - 4, data + off, n, ',');
      ESP_LOGD(TAG, "%s%s", this->debug_prefix_.c_str(), buf);
    }
  }
#endif
  while (len > 0) {
    UsbOutputChunk *chunk = this->output_pool_.allocate();
    if (chunk == nullptr) {
      ESP_LOGE(TAG, "Output pool full - lost %zu bytes", len);
      break;
    }
    uint16_t chunk_len = std::min(len, UsbOutputChunk::MAX_CHUNK_SIZE);
    memcpy(chunk->data, data, chunk_len);
    chunk->length = static_cast<uint8_t>(chunk_len);
    // Push always succeeds: pool is sized to queue capacity (SIZE-1), so if
    // allocate() returned non-null, the queue cannot be full.
    this->output_queue_.push(chunk);
    data += chunk_len;
    len -= chunk_len;
  }
  this->parent_->start_output(this);
}

uart::UARTFlushResult USBUartChannel::flush() {
  // Spin until the output queue is drained and the last USB transfer completes.
  // Safe to call from the main loop only.
  // The flush_timeout_ms_ timeout guards against a device that stops responding mid-flush;
  // in that case the main loop is blocked for the full duration.
  uint32_t start = millis();
  while ((!this->output_queue_.empty() || this->output_started_.load()) && millis() - start < this->flush_timeout_ms_) {
    // Kick start_output() in case data arrived but no transfer is in flight yet.
    this->parent_->start_output(this);
    yield();
  }
  if (!this->output_queue_.empty() || this->output_started_.load())
    return uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
  return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;
}

bool USBUartChannel::peek_byte(uint8_t *data) {
  if (this->input_buffer_.is_empty()) {
    return false;
  }
  *data = this->input_buffer_.peek();
  return true;
}
bool USBUartChannel::read_array(uint8_t *data, size_t len) {
  if (!this->initialised_.load()) {
    ESP_LOGV(TAG, "Channel not initialised - read ignored");
    return false;
  }
  auto available = this->available();
  bool status = true;
  if (len > available) {
    ESP_LOGV(TAG, "underflow: requested %zu but returned %d, bytes", len, available);
    len = available;
    status = false;
  }
  for (size_t i = 0; i != len; i++) {
    *data++ = this->input_buffer_.pop();
  }
  this->parent_->start_input(this);
  return status;
}
void USBUartComponent::setup() { USBClient::setup(); }
void USBUartComponent::loop() {
  bool had_work = this->process_usb_events_();

  // Process USB data from the lock-free queue
  UsbDataChunk *chunk;
  while ((chunk = this->usb_data_queue_.pop()) != nullptr) {
    had_work = true;
    auto *channel = chunk->channel;

#ifdef USE_UART_DEBUGGER
    if (channel->debug_) {
      char buf[4 + format_hex_pretty_size(usb_host::USB_MAX_PACKET_SIZE)];  // "<<< " + hex
      memcpy(buf, "<<< ", 4);
      format_hex_pretty_to(buf + 4, sizeof(buf) - 4, chunk->data, chunk->length, ',');
      ESP_LOGD(TAG, "%s%s", channel->debug_prefix_.c_str(), buf);
    }
#endif

    // Push data to ring buffer (now safe in main loop)
    channel->input_buffer_.push(chunk->data, chunk->length);

    // Return chunk to pool for reuse
    this->chunk_pool_.release(chunk);

    // Invoke the RX callback (if registered) immediately after data lands in the
    // ring buffer.  This lets consumers such as ZigbeeProxy process incoming bytes
    // in the same loop iteration they are delivered, avoiding an extra wakeup cycle.
    if (channel->rx_callback_) {
      channel->rx_callback_();
    }
  }

  // Log dropped USB data periodically
  uint16_t dropped = this->usb_data_queue_.get_and_reset_dropped_count();
  if (dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u USB data chunks due to buffer overflow", dropped);
  }

  // Disable loop when idle. Callbacks re-enable via enable_loop_soon_any_context().
  if (!had_work) {
    this->disable_loop();
  }
}
void USBUartComponent::dump_config() {
  USBClient::dump_config();
  for (auto &channel : this->channels_) {
    ESP_LOGCONFIG(TAG,
                  "  UART Channel %d\n"
                  "    Baud Rate: %" PRIu32 " baud\n"
                  "    Data Bits: %u\n"
                  "    Parity: %s\n"
                  "    Stop bits: %s\n"
                  "    Flush Timeout: %" PRIu32 " ms\n"
                  "    Debug: %s\n"
                  "    Dummy receiver: %s",
                  channel->index_, channel->baud_rate_, channel->data_bits_, PARITY_NAMES[channel->parity_],
                  STOP_BITS_NAMES[channel->stop_bits_], channel->flush_timeout_ms_, YESNO(channel->debug_),
                  YESNO(channel->dummy_receiver_));
  }
}
void USBUartComponent::start_input(USBUartChannel *channel) {
  if (!channel->initialised_.load())
    return;
  // THREAD CONTEXT: Called from both USB task and main loop threads
  // - USB task: Immediate restart after successful transfer for continuous data flow
  // - Main loop: Controlled restart after consuming data (backpressure mechanism)
  //
  // This dual-thread access is intentional for performance:
  // - USB task restarts avoid context switch delays for high-speed data
  // - Main loop restarts provide flow control when buffers are full
  //
  // The underlying transfer_in() uses lock-free atomic allocation from the
  // TransferRequest pool, making this multi-threaded access safe

  // Use compare_exchange_strong to avoid spurious failures: a missed submit here is
  // never retried by read_array() because no data will ever arrive to trigger it.
  auto started = false;
  if (!channel->input_started_.compare_exchange_strong(started, true))
    return;
  const auto *ep = channel->cdc_dev_.in_ep;
  // CALLBACK CONTEXT: This lambda is executed in USB task via transfer_callback
  auto callback = [this, channel](const usb_host::TransferStatus &status) {
    ESP_LOGV(TAG, "Transfer result: length: %u; status %X", status.data_len, status.error_code);
    if (!status.success) {
      ESP_LOGE(TAG, "Input transfer failed, status=%s", esp_err_to_name(status.error_code));
      // On failure, don't restart - let next read_array() trigger it
      channel->input_started_.store(false);
      return;
    }

    if (!channel->dummy_receiver_ && status.data_len > 0) {
      // Allocate a chunk from the pool
      UsbDataChunk *chunk = this->chunk_pool_.allocate();
      if (chunk == nullptr) {
        // No chunks available - queue is full or we're out of memory
        this->usb_data_queue_.increment_dropped_count();
        // Mark input as not started so we can retry
        channel->input_started_.store(false);
        return;
      }

      // Copy data to chunk (this is fast, happens in USB task)
      memcpy(chunk->data, status.data, status.data_len);
      chunk->length = status.data_len;
      chunk->channel = channel;

      // Push to lock-free queue for main loop processing
      // Push always succeeds: pool is sized to queue capacity (SIZE-1), so if
      // allocate() returned non-null, the queue cannot be full.
      this->usb_data_queue_.push(chunk);

      // Re-enable component loop to process the queued data
      this->enable_loop_soon_any_context();

      // Wake main loop immediately to process USB data
      App.wake_loop_threadsafe();
    }

    // On success, restart input immediately from USB task for performance
    // The lock-free queue will handle backpressure
    channel->input_started_.store(false);
    this->start_input(channel);
  };
  if (!this->transfer_in(ep->bEndpointAddress, callback, ep->wMaxPacketSize)) {
    ESP_LOGE(TAG, "IN transfer submission failed for ep=0x%02X", ep->bEndpointAddress);
    channel->input_started_.store(false);
  }
}

void USBUartComponent::start_output(USBUartChannel *channel) {
  // THREAD CONTEXT: Called from both main loop and USB task threads.
  // The output_queue_ is a lock-free SPSC queue, so pop() is safe from either thread.
  // The output_started_ atomic flag is claimed via compare_exchange to guarantee that
  // only one thread starts a transfer at a time.

  // Atomically claim the "output in progress" flag. If already set, another thread
  // is handling the transfer; return immediately.
  bool expected = false;
  if (!channel->output_started_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
    return;
  }

  UsbOutputChunk *chunk = channel->output_queue_.pop();
  if (chunk == nullptr) {
    // Nothing to send — release the flag and return.
    channel->output_started_.store(false, std::memory_order_release);
    return;
  }

  const auto *ep = channel->cdc_dev_.out_ep;
  // CALLBACK CONTEXT: This lambda is executed in the USB task via transfer_callback.
  // It releases the chunk, clears the flag, and directly restarts output without
  // going through defer() — eliminating one full main-loop-wakeup cycle of latency.
  auto callback = [this, channel, chunk](const usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGW(TAG, "Output transfer failed: status %X", status.error_code);
    } else {
      ESP_LOGV(TAG, "Output Transfer result: length: %u; status %X", status.data_len, status.error_code);
    }
    channel->output_pool_.release(chunk);
    channel->output_started_.store(false, std::memory_order_release);
    // Restart directly from USB task — safe because output_queue_ is lock-free
    // and transfer_out() uses thread-safe atomic slot allocation.
    this->start_output(channel);
  };

  const auto len = chunk->length;
  if (!this->transfer_out(ep->bEndpointAddress, callback, chunk->data, len)) {
    // Transfer submission failed — return chunk and release flag so callers can retry.
    channel->output_pool_.release(chunk);
    channel->output_started_.store(false, std::memory_order_release);
    return;
  }
  ESP_LOGV(TAG, "Output %u bytes started", len);
}

/**
 * Hacky fix for some devices that report incorrect MPS values
 * @param ep The endpoint descriptor
 */
static void fix_mps(const usb_ep_desc_t *ep) {
  if (ep != nullptr) {
    auto *ep_mutable = const_cast<usb_ep_desc_t *>(ep);
    if (ep->wMaxPacketSize > usb_host::USB_MAX_PACKET_SIZE) {
      ESP_LOGW(TAG, "Corrected MPS of EP 0x%02X from %u to %u", static_cast<uint8_t>(ep->bEndpointAddress & 0xFF),
               ep->wMaxPacketSize, usb_host::USB_MAX_PACKET_SIZE);
      ep_mutable->wMaxPacketSize = usb_host::USB_MAX_PACKET_SIZE;
    }
  }
}
void USBUartTypeCdcAcm::on_connected() {
  auto cdc_devs = this->parse_descriptors(this->device_handle_);
  if (cdc_devs.empty()) {
    this->status_set_error(LOG_STR("No CDC-ACM device found"));
    this->disconnect();
    return;
  }
  ESP_LOGD(TAG, "Found %zu CDC-ACM devices", cdc_devs.size());
  size_t i = 0;
  for (auto *channel : this->channels_) {
    if (i == cdc_devs.size()) {
      ESP_LOGE(TAG, "No configuration found for channel %d", channel->index_);
      this->status_set_warning(LOG_STR("No configuration found for channel"));
      break;
    }
    channel->cdc_dev_ = cdc_devs[i++];
    fix_mps(channel->cdc_dev_.in_ep);
    fix_mps(channel->cdc_dev_.out_ep);
    channel->initialised_.store(true);
    // Claim the communication (interrupt) interface so CDC class requests are accepted
    // by the device. Some CDC ACM implementations (e.g. EFR32 NCP) require this before
    // they enable data flow on the bulk endpoints.
    if (channel->cdc_dev_.interrupt_interface_number != 0xFF &&
        channel->cdc_dev_.interrupt_interface_number != channel->cdc_dev_.bulk_interface_number) {
      auto err_comm = usb_host_interface_claim(this->handle_, this->device_handle_,
                                               channel->cdc_dev_.interrupt_interface_number, 0);
      if (err_comm != ESP_OK) {
        ESP_LOGW(TAG, "Could not claim comm interface %d: %s", channel->cdc_dev_.interrupt_interface_number,
                 esp_err_to_name(err_comm));
        channel->cdc_dev_.interrupt_interface_number = 0xFF;  // Mark as unavailable, but continue anyway
      } else {
        ESP_LOGD(TAG, "Claimed comm interface %d", channel->cdc_dev_.interrupt_interface_number);
      }
    }
    auto err =
        usb_host_interface_claim(this->handle_, this->device_handle_, channel->cdc_dev_.bulk_interface_number, 0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "usb_host_interface_claim failed: %s, channel=%d, intf=%d", esp_err_to_name(err), channel->index_,
               channel->cdc_dev_.bulk_interface_number);
      this->status_set_error(LOG_STR("usb_host_interface_claim failed"));
      this->disconnect();
      return;
    }
  }
  this->status_clear_error();
  this->enable_channels();
}

void USBUartTypeCdcAcm::on_disconnected() {
  for (auto *channel : this->channels_) {
    if (channel->cdc_dev_.in_ep != nullptr) {
      usb_host_endpoint_halt(this->device_handle_, channel->cdc_dev_.in_ep->bEndpointAddress);
      usb_host_endpoint_flush(this->device_handle_, channel->cdc_dev_.in_ep->bEndpointAddress);
    }
    if (channel->cdc_dev_.out_ep != nullptr) {
      usb_host_endpoint_halt(this->device_handle_, channel->cdc_dev_.out_ep->bEndpointAddress);
      usb_host_endpoint_flush(this->device_handle_, channel->cdc_dev_.out_ep->bEndpointAddress);
    }
    if (channel->cdc_dev_.notify_ep != nullptr) {
      usb_host_endpoint_halt(this->device_handle_, channel->cdc_dev_.notify_ep->bEndpointAddress);
      usb_host_endpoint_flush(this->device_handle_, channel->cdc_dev_.notify_ep->bEndpointAddress);
    }
    if (channel->cdc_dev_.interrupt_interface_number != 0xFF &&
        channel->cdc_dev_.interrupt_interface_number != channel->cdc_dev_.bulk_interface_number) {
      usb_host_interface_release(this->handle_, this->device_handle_, channel->cdc_dev_.interrupt_interface_number);
      channel->cdc_dev_.interrupt_interface_number = 0xFF;
    }
    usb_host_interface_release(this->handle_, this->device_handle_, channel->cdc_dev_.bulk_interface_number);
    // Reset the input and output started flags to their initial state to avoid the possibility of spurious restarts
    channel->input_started_.store(true);
    channel->output_started_.store(true);
    channel->input_buffer_.clear();
    // Drain any pending output chunks and return them to the pool
    {
      UsbOutputChunk *chunk;
      while ((chunk = channel->output_queue_.pop()) != nullptr) {
        channel->output_pool_.release(chunk);
      }
    }
    channel->initialised_.store(false);
  }
  USBClient::on_disconnected();
}

void USBUartTypeCdcAcm::enable_channels() {
  static constexpr uint8_t CDC_REQUEST_TYPE = usb_host::USB_TYPE_CLASS | usb_host::USB_RECIP_INTERFACE;
  static constexpr uint8_t CDC_SET_LINE_CODING = 0x20;
  static constexpr uint8_t CDC_SET_CONTROL_LINE_STATE = 0x22;
  static constexpr uint16_t CDC_DTR_RTS = 0x0003;  // D0=DTR, D1=RTS

  for (auto *channel : this->channels_) {
    if (!channel->initialised_.load())
      continue;
    // Configure the bridge's UART parameters. A USB-UART bridge will not forward data
    // at the correct speed until SET_LINE_CODING is sent; without it the UART may run
    // at an indeterminate default rate so the NCP receives garbled bytes and never
    // sends RSTACK.
    uint32_t baud = channel->baud_rate_;
    std::vector<uint8_t> line_coding = {
        static_cast<uint8_t>(baud & 0xFF),         static_cast<uint8_t>((baud >> 8) & 0xFF),
        static_cast<uint8_t>((baud >> 16) & 0xFF), static_cast<uint8_t>((baud >> 24) & 0xFF),
        static_cast<uint8_t>(channel->stop_bits_),  // bCharFormat: 0=1stop, 1=1.5stop, 2=2stop
        static_cast<uint8_t>(channel->parity_),     // bParityType: 0=None, 1=Odd, 2=Even, 3=Mark, 4=Space
        static_cast<uint8_t>(channel->data_bits_),  // bDataBits
    };
    ESP_LOGD(TAG, "SET_LINE_CODING: baud=%u stop=%u parity=%u data=%u", (unsigned) baud, channel->stop_bits_,
             (unsigned) channel->parity_, channel->data_bits_);
    this->control_transfer(
        CDC_REQUEST_TYPE, CDC_SET_LINE_CODING, 0, channel->cdc_dev_.interrupt_interface_number,
        [](const usb_host::TransferStatus &status) {
          if (!status.success) {
            ESP_LOGW(TAG, "SET_LINE_CODING failed: %X", status.error_code);
          } else {
            ESP_LOGD(TAG, "SET_LINE_CODING OK");
          }
        },
        line_coding);
    // Assert DTR+RTS to signal DTE is present.
    this->control_transfer(CDC_REQUEST_TYPE, CDC_SET_CONTROL_LINE_STATE, CDC_DTR_RTS,
                           channel->cdc_dev_.interrupt_interface_number, [](const usb_host::TransferStatus &status) {
                             if (!status.success) {
                               ESP_LOGW(TAG, "SET_CONTROL_LINE_STATE failed: %X", status.error_code);
                             } else {
                               ESP_LOGD(TAG, "SET_CONTROL_LINE_STATE (DTR+RTS) OK");
                             }
                           });
  }
  this->start_channels();
}

void USBUartTypeCdcAcm::start_channels() {
  for (auto *channel : this->channels_) {
    if (!channel->initialised_.load())
      continue;
    channel->input_started_.store(false);
    channel->output_started_.store(false);
    this->start_input(channel);
  }
}

}  // namespace esphome::usb_uart

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
