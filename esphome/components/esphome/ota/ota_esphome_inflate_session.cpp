#include "ota_esphome.h"
#ifdef USE_OTA
#ifdef USE_OTA_DEFLATE
#include "esphome/components/ota/ota_backend.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {

static const char *const TAG = "esphome.ota";

// The window doubles as the output buffer; flushed bytes stay as back
// reference history for the next windowful.
ota::OTAResponseTypes ESPHomeOTAComponent::inflate_flush_(InflateSession &session) {
  const size_t produced = session.dest - session.window;
  const size_t pending = produced - session.flushed;
  if (pending != 0) {
    if (pending > session.image_size - session.written) {
      ESP_LOGW(TAG, "Inflate overrun");
      return ota::OTA_RESPONSE_ERROR_UNKNOWN;
    }
    ota::OTAResponseTypes result = this->write_flash_(session.window + session.flushed, pending);
    if (result != ota::OTA_RESPONSE_OK)
      return result;
    session.flushed = produced;
    session.written += pending;
    // A compressible region yields many windows per socket read
    App.feed_wdt();
  }
  // Even with nothing new written: a block boundary can fall inside a header
  this->ack_written_(*session.xfer);
  return ota::OTA_RESPONSE_OK;
}

ota::OTAResponseTypes ESPHomeOTAComponent::inflate_data_(uint8_t *in, size_t image_size, DataTransfer &xfer) {
  InflateSession &session = *this->inflate_;
  session.self = this;
  session.xfer = &xfer;
  session.in = in;
  session.image_size = image_size;
  session.written = 0;
  session.error = ota::OTA_RESPONSE_OK;
  ota_inflate_init(&session, session.window, OTA_INFLATE_WINDOW_SIZE);
  // Where the ack must follow the write, flush and ack before waiting for
  // input, or the client waits for an ack while the decoder waits for data
  session.source_read_cb = [](OtaInflateState *d) -> int {
    auto *s = static_cast<InflateSession *>(d);
    if (ACK_AFTER_WRITE) {
      s->error = s->self->inflate_flush_(*s);
      if (s->error != ota::OTA_RESPONSE_OK)
        return -1;
    }
    // More input than announced; reported by the size check below
    if (s->xfer->total >= s->xfer->ota_size)
      return -1;
    ssize_t read = s->self->receive_data_(s->in, *s->xfer);
    if (read <= 0) {
      // Already logged by receive_data_
      s->error = ota::OTA_RESPONSE_ERROR_UNKNOWN;
      return -1;
    }
    d->source = s->in + 1;
    d->source_limit = s->in + read;
    return s->in[0];
  };

  int res;
  do {
    // The ring index wrapped to 0 exactly when the window filled
    session.dest = session.window;
    session.dest_limit = session.window + OTA_INFLATE_WINDOW_SIZE;
    session.flushed = 0;
    res = ota_inflate(&session);
    // A stored block keeps emitting zeros after a failed read, hence eof
    if (res < 0 || session.eof)
      break;
    session.error = this->inflate_flush_(session);
  } while (res != OTA_INFLATE_DONE && session.error == ota::OTA_RESPONSE_OK);

  // Transport and flash failures are logged where they happen
  if (session.error != ota::OTA_RESPONSE_OK)
    return session.error;
  if (res != OTA_INFLATE_DONE || session.written != image_size || xfer.total != xfer.ota_size) {
    ESP_LOGW(TAG, "Inflate err %d, %zu of %zu B from %zu of %zu", res, session.written, image_size, xfer.total,
             xfer.ota_size);
    return ota::OTA_RESPONSE_ERROR_UNKNOWN;
  }
  ESP_LOGD(TAG, "Inflated %zu bytes from %zu", session.written, xfer.total);
  return ota::OTA_RESPONSE_OK;
}

}  // namespace esphome
#endif  // USE_OTA_DEFLATE
#endif  // USE_OTA
