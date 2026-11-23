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

#include "ipp_server.h"

namespace esphome::xerox_phaser_3020 {

/// Memory for the encoder and the raster decoder, given to the component as one buffer
inline constexpr size_t WORK_MEMORY_SIZE =
    BAND_BUFFER_SIZE + QPDL_SCRATCH_SIZE + RASTER_LINE_BUFFER_SIZE + RASTER_WORK_BUFFER_SIZE;

/// Makes a Xerox Phaser 3020 on the USB host port a driverless network printer (IPP Everywhere and
/// AirPrint). Jobs arrive as PWG or Apple raster and go to the printer as QPDL while they are received.
class XeroxPhaser3020 final : public usb_host::USBClient, public PrinterPort {
 public:
  XeroxPhaser3020(uint16_t vid, uint16_t pid, uint8_t *work_memory) : USBClient(vid, pid), work_memory_(work_memory) {}

  void setup() override;
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
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(connected)
  SUB_BINARY_SENSOR(printing)
  SUB_BINARY_SENSOR(paper_empty)
#endif

  // PrinterPort, called from the connection tasks
  bool is_connected() const override { return this->connected_.load(std::memory_order_relaxed); }
  bool is_paper_empty() const override;
  bool begin_job(const std::atomic<bool> *cancel) override;
  bool write(const uint8_t *data, size_t len) override;
  bool end_job() override;
  void abort_job() override;

 protected:
  void on_connected() override;
  void on_disconnected() override;

  void update_();
  void register_mdns_();
  bool job_stopped_() const;
  /// Wait for a free transfer. Returns null when the job has to stop.
  usb_transfer_t *take_transfer_();
  bool submit_(usb_transfer_t *transfer);
  /// Wait until no transfer is in flight, or until `stop_on_cancel` and the job is canceled
  bool wait_idle_(bool stop_on_cancel);
  static void transfer_callback(usb_transfer_t *transfer);
  static void listen_task(void *arg);
  static void worker_task(void *arg);

  IppServer server_;
  ServerSettings settings_{};
  char uuid_[37];
  uint8_t *work_memory_;
  usb_transfer_t *transfers_[2]{};
  usb_transfer_t *current_{nullptr};  // being filled by write()
  QueueHandle_t free_transfers_{nullptr};
  QueueHandle_t connections_{nullptr};
  const std::atomic<bool> *cancel_{nullptr};
  // Held while the endpoint is used from a connection task or torn down by the main loop
  Mutex usb_lock_;
  int listen_fd_{-1};
  std::atomic<bool> connected_{false};
  std::atomic<bool> transfer_failed_{false};
  // Last GET_PORT_STATUS answer (IEEE 1284 bits)
  std::atomic<uint8_t> port_status_{0};
  uint8_t interface_{0};
  uint8_t out_endpoint_{0};
  uint8_t update_count_{0};
};

}  // namespace esphome::xerox_phaser_3020

#endif
