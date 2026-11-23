#pragma once

#include "esphome/core/defines.h"
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)

#include <atomic>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "esphome/components/usb_host/usb_host.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "ipp_server.h"

namespace esphome::xerox_phaser_3020 {

/// Memory for the encoder and the raster decoder other than the band and the compressed band
inline constexpr size_t WORK_MEMORY_SIZE = QPDL_SCRATCH_SIZE + RASTER_LINE_BUFFER_SIZE + RASTER_WORK_BUFFER_SIZE;

/// Makes a Xerox Phaser 3020 on the USB host port a driverless network printer (IPP Everywhere and
/// AirPrint). Jobs arrive as PWG or Apple raster and go to the printer as QPDL while they are received.
class XeroxPhaser3020 final : public usb_host::USBClient, public PrinterPort {
 public:
  XeroxPhaser3020(uint16_t vid, uint16_t pid) : USBClient(vid, pid) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  // After usb_host, and after mdns so the printer's Bonjour service can be added
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_printer_name(const char *name) { this->settings_.name = name; }
  void set_location(const char *location) { this->settings_.location = location; }
  void set_port(uint16_t port) { this->settings_.port = port; }
  void set_paper(uint8_t paper) { this->settings_.default_paper = paper; }
  void set_toner_density(uint8_t density) { this->settings_.pjl.density = density; }
  void set_toner_save(TonerSave toner_save) { this->settings_.pjl.toner_save = toner_save; }
  void set_power_save_minutes(uint8_t minutes) { this->settings_.pjl.power_save_minutes = minutes; }
  void set_jam_recovery(bool jam_recovery) { this->settings_.pjl.jam_recovery = jam_recovery; }
  void set_printer_wait(uint16_t seconds) { this->settings_.printer_wait_s = seconds; }
  /// How often to ask the printer for its status; 0 asks only after jobs and connections
  void set_status_interval(uint32_t ms) { this->status_interval_ms_ = ms; }
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(connected)
  SUB_BINARY_SENSOR(printing)
  SUB_BINARY_SENSOR(paper_empty)
  SUB_BINARY_SENSOR(error)
  SUB_BINARY_SENSOR(online)
  SUB_BINARY_SENSOR(toner_low)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(page_count)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(status)
#endif

  /// Ask the printer for its status with PJL and log the answer. An experiment: what the Phaser 3020
  /// answers is not documented.
  void query_status();

  // PrinterPort, called from the connection tasks
  bool is_connected() const override { return this->connected_.load(std::memory_order_relaxed); }
  bool is_paper_empty() const override;
  bool is_toner_low() const override { return this->toner_low_.load(std::memory_order_relaxed); }
  bool begin_job(const std::atomic<bool> *cancel) override;
  bool write(const uint8_t *data, size_t len) override;
  bool end_job() override;
  void abort_job() override;
  uint32_t wait_ms() const override { return this->wait_us_ / 1000; }

 protected:
  void on_connected() override;
  void on_disconnected() override;

  void update_();
  void register_mdns_();
  void send_reset_();
  void log_interfaces_(const usb_config_desc_t *config);
  void read_device_id_();
  bool submit_in_();
  /// Ask the printer for its status: everything, logged, for the button; status and page count for a poll
  void start_query_(bool full);
  void finish_query_();
  void end_query_(uint8_t tries);
  void parse_status_();
  static void in_callback(usb_transfer_t *transfer);
  static void device_id_callback(usb_transfer_t *transfer);
  bool job_stopped_() const;
  /// Wait for a free transfer. Returns null when the job has to stop.
  usb_transfer_t *take_transfer_();
  bool submit_(usb_transfer_t *transfer);
  /// Wait for a transfer to come back, counting the time as time the printer kept the job waiting
  bool receive_transfer_(usb_transfer_t **transfer, TickType_t timeout);
  /// Wait until no transfer is in flight. Returns false when the job has to stop first.
  bool wait_idle_();
  static void transfer_callback(usb_transfer_t *transfer);
  static void listen_task(void *arg);
  static void worker_task(void *arg);

  IppServer server_;
  ServerSettings settings_{};
  char uuid_[37];
  usb_transfer_t *transfers_[2]{};
  usb_transfer_t *current_{nullptr};  // being filled by write()
  usb_transfer_t *in_transfer_{nullptr};
  usb_transfer_t *id_transfer_{nullptr};
  char *reply_{nullptr};  // what the printer sent back to a query
  char echo_[16];         // the tag a poll asks the printer to echo; empty for the full query
  QueueHandle_t free_transfers_{nullptr};
  QueueHandle_t connections_{nullptr};
  const std::atomic<bool> *cancel_{nullptr};
  // Kept to report in dump_config() where they were placed
  uint8_t *band_{nullptr};
  uint8_t *packed_{nullptr};
  uint8_t *work_{nullptr};
  uint32_t wait_us_{0};  // this job's time waiting for the printer; only the job's task uses it
  // Held while the endpoint is used from a connection task or torn down by the main loop
  Mutex usb_lock_;
  int listen_fd_{-1};
  std::atomic<bool> connected_{false};
  std::atomic<bool> transfer_failed_{false};
  // A printer reset after an interrupted job: asked for by a connection task, sent by the main loop
  std::atomic<uint8_t> reset_state_{0};
  // Last GET_PORT_STATUS answer (IEEE 1284 bits)
  std::atomic<uint8_t> port_status_{0};
  std::atomic<uint16_t> reply_len_{0};
  std::atomic<uint8_t> query_state_{0};
  std::atomic<bool> in_busy_{false};     // the IN transfer is submitted
  std::atomic<bool> id_ready_{false};    // the device ID has arrived and waits to be logged
  std::atomic<bool> query_done_{false};  // a poll has all its answers and waits to be finished
  std::atomic<bool> toner_low_{false};
  uint32_t status_interval_ms_{0};
  uint16_t poll_number_{0};
  uint8_t answers_needed_{0};  // a poll ends early once this many answers are in; 0 reads until the time is up
  bool query_full_{false};
  bool was_printing_{false};
  uint8_t interface_{0};
  uint8_t out_endpoint_{0};
  uint8_t in_endpoint_{0};  // 0 when the printer cannot send data back
  uint8_t update_count_{0};
};

}  // namespace esphome::xerox_phaser_3020

#endif
