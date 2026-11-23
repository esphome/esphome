#pragma once

// A small IPP Everywhere / AirPrint print server. It speaks HTTP/1.1 and IPP 2.0 over plain sockets,
// keeps a short list of recent jobs and prints one job at a time, straight from the network to the
// printer: documents are never stored.
//
// The server has no threads of its own. The owner accepts connections on listen_socket() and calls
// serve() for each, from as many tasks as it wants connections to be handled at once.

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "esphome/core/helpers.h"

#include "qpdl.h"
#include "raster.h"

namespace esphome::xerox_phaser_3020 {

/// Document formats accepted, as listed in IPP and in the Bonjour TXT record
inline constexpr char DOCUMENT_FORMATS[] = "application/octet-stream,image/pwg-raster,image/urf";
/// Apple raster capabilities: grey and colour, 99 copies, 300 and 600 dpi, three qualities
inline constexpr char URF_SUPPORTED[] = "V1.4,W8,SRGB24,CP99,RS300-600,PQ3-4-5,MT1,IS1";

/// The printer end of the server
class PrinterPort : public OutputSink {
 public:
  virtual bool is_connected() const = 0;
  virtual bool is_paper_empty() const = 0;
  /// The printer has reported low toner
  virtual bool is_toner_low() const { return false; }
  /// Get ready for a job's data. write() and end_job() return false once `cancel` is set.
  virtual bool begin_job(const std::atomic<bool> *cancel) = 0;
  /// Wait until all data has been taken by the printer. Returns false on an error or a cancel.
  virtual bool end_job() = 0;
  /// Throw away data the printer has not taken yet and reset the printer's port
  virtual void abort_job() = 0;
  /// Time this job has spent waiting for the printer to take data, in milliseconds
  virtual uint32_t wait_ms() const = 0;
};

struct ServerSettings {
  const char *name;      // printer-name, also the Bonjour name
  const char *location;  // printer-location, may be empty
  const char *uuid;      // 36 characters, without "urn:uuid:"
  PjlSettings pjl;
  uint16_t port;
  uint16_t printer_wait_s;  // how long a job waits for the printer to be switched on or plugged in
  uint8_t default_paper;
};

enum class JobReason : uint8_t {
  JOB_REASON_NONE,
  JOB_REASON_INCOMING,
  JOB_REASON_PRINTING,
  JOB_REASON_WAITING_FOR_PRINTER,
  JOB_REASON_COMPLETED,
  JOB_REASON_CANCELED,
  JOB_REASON_FORMAT_ERROR,
  JOB_REASON_UNSUPPORTED,
  JOB_REASON_PRINTER_ERROR,
  JOB_REASON_PRINTER_MISSING,
  JOB_REASON_CLIENT_GONE,
  JOB_REASON_TIMED_OUT,
};

struct Job {
  uint32_t id{0};  // 0 marks a free slot
  uint32_t created{0};
  uint32_t processing{0};
  uint32_t completed{0};
  uint16_t pages{0};
  uint16_t copies{1};
  uint8_t state{0};
  JobReason reason{JobReason::JOB_REASON_NONE};
  bool has_document{false};
  char name[48];
  char user[32];
};

struct IppRequest;
class HttpConnection;
class IppWriter;

class IppServer {
 public:
  static constexpr uint8_t MAX_JOBS = 8;

  /// Buffers: band, scratch and packed (may be null) for QpdlEncoder, line and work for RasterPrinter
  void setup(const ServerSettings &settings, PrinterPort *port, uint8_t *band, uint8_t *scratch, uint8_t *packed,
             uint8_t *line, uint8_t *work);
  /// Open the listening socket. Returns the socket, or -1 on failure.
  int listen_socket();
  /// Handle one client connection until it ends, then close it. May run in several tasks at once.
  void serve(int fd);
  /// Give up on a job created with Create-Job whose document never came. Call about once a second.
  void check_timeouts();

  bool is_printing() const { return this->printing_.load(std::memory_order_relaxed); }
  const ServerSettings &settings() const { return this->settings_; }

 protected:
  void handle_ipp_(HttpConnection &conn);
  void handle_status_page_(HttpConnection &conn);

  void op_get_printer_attributes_(IppWriter &w, const IppRequest &req, HttpConnection &conn);
  void op_print_job_(IppWriter &w, IppRequest &req, HttpConnection &conn);
  void op_create_job_(IppWriter &w, IppRequest &req);
  void op_send_document_(IppWriter &w, IppRequest &req, HttpConnection &conn);
  void op_close_job_(IppWriter &w, IppRequest &req);
  void op_cancel_job_(IppWriter &w, IppRequest &req);
  void op_cancel_my_jobs_(IppWriter &w, IppRequest &req);
  void op_get_job_attributes_(IppWriter &w, IppRequest &req);
  void op_get_jobs_(IppWriter &w, IppRequest &req);

  /// IPP status for a job request whose document format or compression cannot be printed, or 0
  uint16_t check_document_(const IppRequest &req) const;
  /// Reserve the printer for a new job and keep its job template settings for its document.
  /// Returns the job's slot, or -1 when another job holds the printer.
  int new_job_(const IppRequest &req, uint8_t state);
  /// Print the document that follows the request, for a job the caller has set to processing.
  /// Returns the IPP status of the request.
  uint16_t print_document_(int slot, IppRequest &req, HttpConnection &conn);
  void finish_job_(int slot, uint8_t state, JobReason reason);
  /// finish_job_() for a caller that holds lock_
  void finish_job_locked_(int slot, uint8_t state, JobReason reason);
  int find_job_(const IppRequest &req) const;
  /// Write the job attributes the request asks for. Takes the lock.
  void write_job_(IppWriter &w, const IppRequest &req, int slot, const char *uri_base);
  void write_job_status_(IppWriter &w, int slot, const char *uri_base);
  uint8_t printer_state_() const;

  ServerSettings settings_{};
  PrinterPort *port_{nullptr};
  QpdlEncoder encoder_;
  RasterPrinter raster_;
  mutable Mutex lock_;
  Job jobs_[MAX_JOBS];
  uint32_t next_id_{1};
  uint32_t doc_deadline_{0};  // when a Create-Job job without a document is given up
  uint32_t state_changed_{1};
  std::atomic<bool> cancel_{false};
  std::atomic<bool> printing_{false};
  std::atomic<uint16_t> pages_{0};  // pages of the active job printed so far
  // Job template settings of the active job, from Print-Job or Create-Job
  RasterJob job_template_{};
  int8_t active_{-1};  // slot of the job holding the printer
  bool job_draft_{false};
};

}  // namespace esphome::xerox_phaser_3020
