#include "xerox_phaser_3020.h"
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)

#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <freertos/task.h>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#ifdef USE_MDNS
#include <mdns.h>
#endif

namespace esphome::xerox_phaser_3020 {

ESPHOME_LOG_TAG(TAG, "xerox_phaser_3020");

// USB printer class requests (USB Device Class Definition for Printing Devices 1.1)
static constexpr uint8_t PRINTER_GET_PORT_STATUS = 1;
static constexpr uint8_t PRINTER_SOFT_RESET = 2;
static constexpr uint8_t PORT_STATUS_PAPER_EMPTY = 0x20;
static constexpr uint8_t PORT_STATUS_READY = 0x18;  // selected, no error

static constexpr size_t TRANSFER_SIZE = 4096;
static constexpr UBaseType_t NUM_TRANSFERS = 2;
// One task can stream a job while the others answer the status requests clients send meanwhile
static constexpr uint8_t NUM_WORKERS = 3;
static constexpr uint32_t WORKER_STACK_SIZE = 8192;
static constexpr uint32_t LISTEN_STACK_SIZE = 3072;
// Encoding a page takes the CPU for seconds. At the idle task's priority it only gets what nothing
// else needs, and the idle task still gets turns to feed the task watchdog.
static constexpr UBaseType_t WORKER_PRIORITY = tskIDLE_PRIORITY;
static constexpr UBaseType_t LISTEN_PRIORITY = tskIDLE_PRIORITY + 1;
static constexpr uint32_t WAIT_SLICE_MS = 200;
static constexpr uint32_t ABORT_WAIT_MS = 2000;
// Internal RAM the band buffer must leave free for everything else (Wi-Fi, sockets, other components)
static constexpr size_t INTERNAL_HEADROOM = 96 * 1024;
static constexpr uint32_t RESET_WAIT_MS = 3000;
static constexpr uint8_t RESET_NONE = 0;
static constexpr uint8_t RESET_REQUESTED = 1;
static constexpr uint8_t RESET_SENT = 2;
static constexpr uint32_t UPDATE_INTERVAL_MS = 1000;
static constexpr uint8_t PORT_STATUS_EVERY = 5;  // updates between port status reads
// wLength of GET_PORT_STATUS; the content is not sent
static const std::vector<uint8_t> PORT_STATUS_LENGTH(1);

void XeroxPhaser3020::setup() {
  USBClient::setup();
  if (this->is_failed())
    return;
  // Kept for the life of the device; PSRAM is far slower than internal RAM. The line buffers (25 kB) are
  // touched for every pixel, so they go to internal RAM whenever it has room. The band (80 kB), written a
  // byte column at a time, goes there too when enough is left for everything else.
  this->work_ = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::PREFER_INTERNAL).allocate(WORK_MEMORY_SIZE);
  const uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  if (heap_caps_get_largest_free_block(internal) >= BAND_BUFFER_SIZE &&
      heap_caps_get_free_size(internal) >= BAND_BUFFER_SIZE + INTERNAL_HEADROOM)
    this->band_ = static_cast<uint8_t *>(heap_caps_malloc(BAND_BUFFER_SIZE, internal));
  if (this->band_ == nullptr)
    this->band_ = RAMAllocator<uint8_t>().allocate(BAND_BUFFER_SIZE);
  if (this->band_ == nullptr || this->work_ == nullptr) {
    this->mark_failed(LOG_STR("Not enough memory"));
    return;
  }
  // With PSRAM, each band is compressed once and kept until it is sent; without, it is compressed twice
  this->packed_ = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::ALLOC_EXTERNAL).allocate(QPDL_PACKED_BUFFER_SIZE);
  this->free_transfers_ = xQueueCreate(NUM_TRANSFERS, sizeof(usb_transfer_t *));
  this->connections_ = xQueueCreate(NUM_WORKERS, sizeof(int));
  if (this->free_transfers_ == nullptr || this->connections_ == nullptr) {
    this->mark_failed(LOG_STR("Not enough memory"));
    return;
  }
  for (auto *&transfer : this->transfers_) {
    if (usb_host_transfer_alloc(TRANSFER_SIZE, 0, &transfer) != ESP_OK) {
      this->mark_failed(LOG_STR("Transfer alloc failed"));
      return;
    }
    xQueueSend(this->free_transfers_, &transfer, 0);
  }

  uint8_t mac[6];
  get_mac_address_raw(mac);
  snprintf(this->uuid_, sizeof(this->uuid_), "a5b3c0de-3020-4e5a-8f1c-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
  this->settings_.uuid = this->uuid_;

  uint8_t *line = this->work_ + QPDL_SCRATCH_SIZE;
  uint8_t *work = line + RASTER_LINE_BUFFER_SIZE;
  this->server_.setup(this->settings_, this, this->band_, this->work_, this->packed_, line, work);
  this->listen_fd_ = this->server_.listen_socket();
  if (this->listen_fd_ < 0) {
    this->mark_failed(LOG_STR("Could not open the IPP port"));
    return;
  }
  if (xTaskCreate(listen_task, "ipp_listen", LISTEN_STACK_SIZE, this, LISTEN_PRIORITY, nullptr) != pdPASS) {
    this->mark_failed(LOG_STR("Task creation failed"));
    return;
  }
  for (uint8_t i = 0; i < NUM_WORKERS; i++) {
    if (xTaskCreate(worker_task, "ipp_worker", WORKER_STACK_SIZE, this, WORKER_PRIORITY, nullptr) != pdPASS) {
      this->mark_failed(LOG_STR("Task creation failed"));
      return;
    }
  }
  this->register_mdns_();
  this->set_interval(UPDATE_INTERVAL_MS, [this]() { this->update_(); });
}

void XeroxPhaser3020::loop() {
  bool work = this->process_usb_events_();
  if (this->reset_state_.load() == RESET_REQUESTED) {
    this->send_reset_();
    work = true;
  }
  if (!work)
    this->disable_loop();
}

void XeroxPhaser3020::send_reset_() {
  // Control transfers are only started from the main loop, which also handles disconnects
  if (!this->is_connected()) {
    this->reset_state_.store(RESET_NONE);
    return;
  }
  this->reset_state_.store(RESET_SENT);
  const bool sent = this->control_transfer(
      usb_host::USB_DIR_OUT | usb_host::USB_TYPE_CLASS | usb_host::USB_RECIP_INTERFACE, PRINTER_SOFT_RESET, 0,
      this->interface_, [this](const usb_host::TransferStatus &) { this->reset_state_.store(RESET_NONE); });
  if (sent) {
    ESP_LOGI(TAG, "Printer reset after an interrupted job");
  } else {
    this->reset_state_.store(RESET_NONE);
  }
}

void XeroxPhaser3020::dump_config() {
  const ServerSettings &s = this->settings_;
  ESP_LOGCONFIG(TAG,
                "Xerox Phaser 3020:\n"
                "  Printer name: %s\n"
                "  Location: %s\n"
                "  URL: ipp://%s.local:%u/ipp/print\n"
                "  Paper: %s\n"
                "  Toner density: %u, toner save: %s\n"
                "  Power save: %u min (0 = off)\n"
                "  Waits %u s for the printer",
                s.name, s.location, App.get_name().c_str(), s.port, PAPERS[s.default_paper].ppd_name, s.pjl.density,
                s.pjl.toner_save == TonerSave::TONER_SAVE_ON    ? LOG_STR_LITERAL("on")
                : s.pjl.toner_save == TonerSave::TONER_SAVE_OFF ? LOG_STR_LITERAL("off")
                                                                : LOG_STR_LITERAL("printer default"),
                s.pjl.power_save_minutes, s.printer_wait_s);
  ESP_LOGCONFIG(TAG,
                "  Band buffer: %s, line buffers: %s, each band compressed %s\n"
                "  Free internal RAM: %u bytes",
                esp_ptr_external_ram(this->band_) ? LOG_STR_LITERAL("PSRAM") : LOG_STR_LITERAL("internal RAM"),
                esp_ptr_external_ram(this->work_) ? LOG_STR_LITERAL("PSRAM") : LOG_STR_LITERAL("internal RAM"),
                this->packed_ != nullptr ? LOG_STR_LITERAL("once") : LOG_STR_LITERAL("twice (no PSRAM)"),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
  USBClient::dump_config();
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Printing", this->printing_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Paper empty", this->paper_empty_binary_sensor_);
#endif
}

void XeroxPhaser3020::register_mdns_() {
#ifdef USE_MDNS
  const ServerSettings &s = this->settings_;
  char admin_url[96];
  snprintf(admin_url, sizeof(admin_url), "http://%s.local:%u/", App.get_name().c_str(), s.port);
  // Keys from the AirPrint and IPP Everywhere (PWG 5100.14) Bonjour specifications
  mdns_txt_item_t txt[] = {
      {"txtvers", "1"},
      {"qtotal", "1"},
      {"rp", "ipp/print"},
      {"ty", "Xerox Phaser 3020"},
      {"product", "(Phaser 3020)"},
      {"usb_MFG", "Xerox"},
      {"usb_MDL", "Phaser 3020"},
      {"note", s.location},
      {"adminurl", admin_url},
      {"pdl", DOCUMENT_FORMATS},
      {"URF", URF_SUPPORTED},
      {"UUID", this->uuid_},
      {"Color", "F"},
      {"Duplex", "F"},
      {"Copies", "T"},
      {"Collate", "F"},
      {"kind", "document"},
      {"PaperMax", "legal-A4"},
      {"priority", "50"},
  };
  esp_err_t err = mdns_service_add(s.name, "_ipp", "_tcp", s.port, txt, sizeof(txt) / sizeof(txt[0]));
  // iOS finds AirPrint printers through the _universal subtype
  if (err == ESP_OK)
    err = mdns_service_subtype_add_for_host(s.name, "_ipp", "_tcp", nullptr, "_universal");
  if (err == ESP_OK)
    err = mdns_service_subtype_add_for_host(s.name, "_ipp", "_tcp", nullptr, "_print");
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Bonjour registration failed (%s); add the printer by its URL", esp_err_to_name(err));
  }
#else
  ESP_LOGW(TAG, "mdns is disabled: devices will not find the printer by themselves");
#endif
}

void XeroxPhaser3020::update_() {
  this->server_.check_timeouts();
  if (++this->update_count_ >= PORT_STATUS_EVERY && this->is_connected()) {
    this->update_count_ = 0;
    this->control_transfer(
        usb_host::USB_DIR_IN | usb_host::USB_TYPE_CLASS | usb_host::USB_RECIP_INTERFACE, PRINTER_GET_PORT_STATUS, 0,
        this->interface_,
        [this](const usb_host::TransferStatus &status) {
          // The data follows the setup packet
          if (status.success && status.data_len > usb_host::SETUP_PACKET_SIZE)
            this->port_status_.store(status.data[usb_host::SETUP_PACKET_SIZE]);
        },
        PORT_STATUS_LENGTH);
  }
#ifdef USE_BINARY_SENSOR
  PUBLISH_STATE(this->connected_binary_sensor_, this->is_connected());
  PUBLISH_STATE(this->printing_binary_sensor_, this->server_.is_printing());
  PUBLISH_STATE(this->paper_empty_binary_sensor_, this->is_paper_empty());
#endif
}

bool XeroxPhaser3020::is_paper_empty() const {
  return this->is_connected() && (this->port_status_.load(std::memory_order_relaxed) & PORT_STATUS_PAPER_EMPTY) != 0;
}

void XeroxPhaser3020::on_connected() {
  const usb_config_desc_t *config;
  if (usb_host_get_active_config_descriptor(this->device_handle_, &config) != ESP_OK) {
    ESP_LOGW(TAG, "Could not read the USB configuration");
    this->disconnect();
    return;
  }
  // The printer class interface and its bulk OUT endpoint
  int interface = -1;
  uint8_t endpoint = 0;
  for (uint8_t i = 0; i < config->bNumInterfaces && interface < 0; i++) {
    int offset = 0;
    const usb_intf_desc_t *intf = usb_parse_interface_descriptor(config, i, 0, &offset);
    if (intf == nullptr || intf->bInterfaceClass != USB_CLASS_PRINTER)
      continue;
    for (uint8_t e = 0; e < intf->bNumEndpoints; e++) {
      int ep_offset = offset;
      const usb_ep_desc_t *ep = usb_parse_endpoint_descriptor_by_index(intf, e, config->wTotalLength, &ep_offset);
      if (ep != nullptr && (ep->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_BULK &&
          (ep->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) == 0) {
        interface = intf->bInterfaceNumber;
        endpoint = ep->bEndpointAddress;
        break;
      }
    }
  }
  if (interface < 0) {
    ESP_LOGW(TAG, "USB device is not a printer");
    this->disconnect();
    return;
  }
  const esp_err_t err = usb_host_interface_claim(this->handle_, this->device_handle_, interface, 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Could not claim the printer interface: %s", esp_err_to_name(err));
    this->disconnect();
    return;
  }
  {
    LockGuard guard(this->usb_lock_);
    this->interface_ = interface;
    this->out_endpoint_ = endpoint;
    this->port_status_.store(PORT_STATUS_READY);
    this->connected_.store(true);
  }
  this->update_count_ = PORT_STATUS_EVERY;  // read the port status on the next update
  ESP_LOGI(TAG, "Printer connected");
}

void XeroxPhaser3020::on_disconnected() {
  {
    LockGuard guard(this->usb_lock_);
    if (this->connected_.exchange(false)) {
      // Ends the transfers in flight; their callbacks return them to the free queue
      usb_host_endpoint_halt(this->device_handle_, this->out_endpoint_);
      usb_host_endpoint_flush(this->device_handle_, this->out_endpoint_);
      usb_host_interface_release(this->handle_, this->device_handle_, this->interface_);
      this->out_endpoint_ = 0;
      this->reset_state_.store(RESET_NONE);
      ESP_LOGI(TAG, "Printer disconnected");
    }
  }
  USBClient::on_disconnected();
}

bool XeroxPhaser3020::job_stopped_() const {
  return this->cancel_->load(std::memory_order_relaxed) || this->transfer_failed_.load(std::memory_order_relaxed) ||
         !this->is_connected();
}

bool XeroxPhaser3020::begin_job(const std::atomic<bool> *cancel) {
  this->cancel_ = cancel;
  // An earlier job's recovery finishes first: its transfers come back and the printer is reset
  for (uint32_t waited = 0;
       waited < RESET_WAIT_MS && this->is_connected() &&
       (uxQueueMessagesWaiting(this->free_transfers_) < NUM_TRANSFERS || this->reset_state_.load() != RESET_NONE);
       waited += 20)
    delay(20);  // NOLINT: runs on a connection task, not the main loop
  this->transfer_failed_.store(false);
  this->current_ = nullptr;
  this->wait_us_ = 0;
  return this->is_connected();
}

usb_transfer_t *XeroxPhaser3020::take_transfer_() {
  usb_transfer_t *transfer = nullptr;
  // The printer holds transfers back while it is busy or out of paper; keep checking for a cancel
  while (!this->receive_transfer_(&transfer, pdMS_TO_TICKS(WAIT_SLICE_MS))) {
    if (this->job_stopped_())
      return nullptr;
  }
  if (this->job_stopped_()) {
    xQueueSend(this->free_transfers_, &transfer, 0);
    return nullptr;
  }
  transfer->num_bytes = 0;
  return transfer;
}

bool XeroxPhaser3020::submit_(usb_transfer_t *transfer) {
  LockGuard guard(this->usb_lock_);
  if (this->is_connected()) {
    transfer->device_handle = this->device_handle_;
    transfer->bEndpointAddress = this->out_endpoint_;
    transfer->callback = transfer_callback;
    transfer->context = this;
    transfer->timeout_ms = 0;
    const esp_err_t err = usb_host_transfer_submit(transfer);
    if (err == ESP_OK)
      return true;
    ESP_LOGW(TAG, "USB transfer failed: %s", esp_err_to_name(err));
  }
  this->transfer_failed_.store(true);
  xQueueSend(this->free_transfers_, &transfer, 0);
  return false;
}

// CALLBACK CONTEXT: USB task
void XeroxPhaser3020::transfer_callback(usb_transfer_t *transfer) {
  auto *self = static_cast<XeroxPhaser3020 *>(transfer->context);
  if (transfer->status != USB_TRANSFER_STATUS_COMPLETED)
    self->transfer_failed_.store(true);
  xQueueSend(self->free_transfers_, &transfer, 0);
}

bool XeroxPhaser3020::write(const uint8_t *data, size_t len) {
  while (len > 0) {
    if (this->current_ == nullptr && (this->current_ = this->take_transfer_()) == nullptr)
      return false;
    usb_transfer_t *transfer = this->current_;
    size_t n = TRANSFER_SIZE - transfer->num_bytes;
    if (n > len)
      n = len;
    memcpy(transfer->data_buffer + transfer->num_bytes, data, n);
    transfer->num_bytes += n;
    data += n;
    len -= n;
    if (transfer->num_bytes == static_cast<int>(TRANSFER_SIZE)) {
      this->current_ = nullptr;
      if (!this->submit_(transfer))
        return false;
    }
  }
  return true;
}

bool XeroxPhaser3020::receive_transfer_(usb_transfer_t **transfer, TickType_t timeout) {
  const uint32_t start = micros();
  const bool received = xQueueReceive(this->free_transfers_, transfer, timeout) == pdTRUE;
  this->wait_us_ += micros() - start;
  return received;
}

bool XeroxPhaser3020::wait_idle_() {
  const uint32_t start = micros();
  while (uxQueueMessagesWaiting(this->free_transfers_) < NUM_TRANSFERS) {
    if (this->job_stopped_())
      return false;
    delay(20);
  }
  this->wait_us_ += micros() - start;
  return true;
}

bool XeroxPhaser3020::end_job() {
  usb_transfer_t *transfer = this->current_;
  this->current_ = nullptr;
  if (transfer != nullptr) {
    if (transfer->num_bytes == 0) {
      xQueueSend(this->free_transfers_, &transfer, 0);
    } else if (!this->submit_(transfer)) {
      return false;
    }
  }
  return this->wait_idle_() && !this->transfer_failed_.load();
}

void XeroxPhaser3020::abort_job() {
  if (this->current_ != nullptr) {
    xQueueSend(this->free_transfers_, &this->current_, 0);
    this->current_ = nullptr;
  }
  {
    LockGuard guard(this->usb_lock_);
    if (!this->is_connected())
      return;
    usb_host_endpoint_halt(this->device_handle_, this->out_endpoint_);
    usb_host_endpoint_flush(this->device_handle_, this->out_endpoint_);
  }
  for (uint32_t waited = 0; uxQueueMessagesWaiting(this->free_transfers_) < NUM_TRANSFERS && waited < ABORT_WAIT_MS;
       waited += 20)
    delay(20);
  LockGuard guard(this->usb_lock_);
  if (!this->is_connected())
    return;
  usb_host_endpoint_clear(this->device_handle_, this->out_endpoint_);
  // Make the printer drop the part of the job it already has; the main loop sends the request
  this->reset_state_.store(RESET_REQUESTED);
  this->enable_loop_soon_any_context();
  App.wake_loop_threadsafe();
}

void XeroxPhaser3020::listen_task(void *arg) {
  auto *self = static_cast<XeroxPhaser3020 *>(arg);
  for (;;) {
    int fd = accept(self->listen_fd_, nullptr, nullptr);
    if (fd < 0) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    // Every connection task is busy and the queue is full
    if (xQueueSend(self->connections_, &fd, 0) != pdTRUE)
      close(fd);
  }
}

void XeroxPhaser3020::worker_task(void *arg) {
  auto *self = static_cast<XeroxPhaser3020 *>(arg);
  for (;;) {
    int fd;
    if (xQueueReceive(self->connections_, &fd, portMAX_DELAY) == pdTRUE)
      self->server_.serve(fd);
  }
}

}  // namespace esphome::xerox_phaser_3020

#endif
