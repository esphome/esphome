#include "canopen.h"

#ifdef USE_CANOPEN_SDO_CLIENT

#include <algorithm>
#include <cstring>

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen.sdo");

// after an abort, the next request (to any node) waits this long, so the server has processed the abort
static constexpr uint32_t ABORT_GUARD_US = 50000;

// indexed by SDOResult; the last entry for other values
PROGMEM_STRING_TABLE(SDOResultStrings, "success", "abort", "timeout", "size mismatch", "transport error", "rejected",
                     "unknown");

const LogString *sdo_result_to_string(SDOResult result) {
  return SDOResultStrings::get_log_str(static_cast<uint8_t>(result), SDOResultStrings::LAST_INDEX);
}

void CANopenComponent::add_sdo_client_od_() {
  // read only: start_sdo_() sets the channel for every request (CO_SDOclient_setup()), so a write wouldn't take
  // effect (no CO_CONFIG_FLAG_OD_DYNAMIC)
  this->od_.add_record(0x1280, {{0, ODA_SDO_R, &this->sdo_cli_highest_sub_, 1},
                                {1, ODA_SDO_R, &this->sdo_cli_cob_tx_, 4},
                                {2, ODA_SDO_R, &this->sdo_cli_cob_rx_, 4},
                                {3, ODA_SDO_R, &this->sdo_cli_node_, 1}});
}

void CANopenComponent::config_sdo_client_(CO_config_t &cfg) {
  cfg.CNT_SDO_CLI = 1;
  cfg.ENTRY_H1280 = this->od_.find(0x1280);
}

// Client-only hub; a node's client is set up by CO_CANopenInit(). Without our own node id there are no local
// transfers, so the client needs no OD.
bool CANopenComponent::init_sdo_client_() {
  uint32_t err_info = 0;
  CO_ReturnError_t err =
      CO_SDOclient_init(this->co_->SDOclient, nullptr, this->od_.find(0x1280), NO_NODE_ID, this->co_->CANmodule,
                        this->co_->RX_IDX_SDO_CLI, this->co_->CANmodule, this->co_->TX_IDX_SDO_CLI, &err_info);
  if (err != CO_ERROR_NO) {
    ESP_LOGE(TAG, "CO_SDOclient_init failed: %d (OD 0x%04" PRIx32 ")", err, err_info);
    return false;
  }
  return true;
}

static void reject(uint8_t node_id, uint16_t index, uint8_t sub, SDOCallback callback, void *context) {
  ESP_LOGW(TAG, "request to node 0x%02x, 0x%04x:%u rejected", node_id, index, sub);
  if (callback != nullptr)
    callback(context, SDOReply{SDOResult::SDO_RESULT_REJECTED, 0, 0, 0, nullptr, 0});
}

// unsigned or sign-extended, as validate_sdo_value() allows
static bool value_fits(uint32_t value, uint8_t size) {
  if (size == 4)
    return true;
  const uint32_t high = value >> (8 * size);
  return high == 0 || (high == (UINT32_MAX >> (8 * size)) && ((value >> (8 * size - 1)) & 1) != 0);
}

bool CANopenComponent::read_sdo(uint8_t node_id, uint16_t index, uint8_t sub, uint8_t expected_size,
                                SDOCallback callback, void *context) {
  if (expected_size != 0 && expected_size != 1 && expected_size != 2 && expected_size != 4) {
    reject(node_id, index, sub, callback, context);
    return false;
  }
  return this->queue_sdo_(
      SDORequest{node_id, sub, SDOKind::SDO_KIND_READ, expected_size, index, 0, {}, callback, context});
}

bool CANopenComponent::write_sdo(uint8_t node_id, uint16_t index, uint8_t sub, uint32_t value, uint8_t size,
                                 SDOCallback callback, void *context) {
  if ((size != 1 && size != 2 && size != 4) || !value_fits(value, size)) {
    reject(node_id, index, sub, callback, context);
    return false;
  }
  if (size < 4)
    value &= (1u << (8 * size)) - 1;  // sign-extended: the reply's value is zero-extended
  return this->queue_sdo_(
      SDORequest{node_id, sub, SDOKind::SDO_KIND_WRITE, size, index, 0, {value}, callback, context});
}

bool CANopenComponent::read_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, uint8_t *data, size_t capacity,
                                     SDOCallback callback, void *context) {
  if (data == nullptr || capacity == 0) {
    reject(node_id, index, sub, callback, context);
    return false;
  }
  // a longer reply aborts anyway, so the buffer's first 64 KiB are enough
  const auto cap = static_cast<uint16_t>(std::min<size_t>(capacity, UINT16_MAX));
  SDORequest request{node_id, sub, SDOKind::SDO_KIND_READ_DATA, 0, index, cap, {}, callback, context};
  request.data = data;
  return this->queue_sdo_(request);
}

bool CANopenComponent::write_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, const uint8_t *data, size_t len,
                                      SDOCallback callback, void *context) {
  if (data == nullptr || len == 0 || len > UINT16_MAX) {
    reject(node_id, index, sub, callback, context);
    return false;
  }
  const auto size = static_cast<uint16_t>(len);
  SDORequest request{node_id, sub, SDOKind::SDO_KIND_WRITE_DATA, 0, index, size, {}, callback, context};
  request.data = const_cast<uint8_t *>(data);  // a WRITE_DATA request only reads it
  return this->queue_sdo_(request);
}

bool CANopenComponent::queue_sdo_(const SDORequest &request) {
  // before setup() the request waits in the queue
  if (this->is_failed() || request.node_id == 0 || request.node_id > 127 || !this->sdo_queue_.push(request)) {
    reject(request.node_id, request.index, request.sub, request.callback, request.context);
    return false;
  }
  this->enable_loop();
  return true;
}

void CANopenComponent::reject_queued_sdo_() {
  while (!this->sdo_queue_.empty()) {
    SDORequest r = this->sdo_queue_.front();
    this->sdo_queue_.pop();
    reject(r.node_id, r.index, r.sub, r.callback, r.context);
  }
}

// A late reply to an ended request would fail the current one: CANopenNode takes any abort and aborts on an
// initiate response for another object.
bool CANopenComponent::sdo_reply_is_stale_(const uint8_t *data, uint8_t len) const {
  if (!this->sdo_busy_ || len < 4)
    return false;
  const CO_SDOclient_t *cli = this->co_->SDOclient;
  // a request that hasn't gone out yet has no reply
  if (cli->CANtxBuff->bufferFull)
    return true;
  const uint8_t scs = data[0] >> 5;  // 0/1: segment responses, 2/3: initiate upload/download responses, 4: abort
  if (scs < 2)                       // late while waiting for the initiate response
    return cli->state == CO_SDO_ST_UPLOAD_INITIATE_RSP || cli->state == CO_SDO_ST_DOWNLOAD_INITIATE_RSP;
  if (scs > 4)
    return false;
  // an initiate response for the other direction, e.g. a late write ack while a read of the object waits
  if ((scs == 2 && cli->state == CO_SDO_ST_DOWNLOAD_INITIATE_RSP) ||
      (scs == 3 && cli->state == CO_SDO_ST_UPLOAD_INITIATE_RSP))
    return true;
  return encode_uint16(data[2], data[1]) != this->sdo_active_.index || data[3] != this->sdo_active_.sub;
}

void CANopenComponent::cancel_sdo(void *context) {
  // one turn of the ring keeps the other requests in order
  for (size_t n = this->sdo_queue_.size(); n > 0; n--) {
    SDORequest r = this->sdo_queue_.front();
    this->sdo_queue_.pop();
    if (r.context != context)
      this->sdo_queue_.push(r);
  }
  if (this->sdo_busy_ && this->sdo_active_.context == context) {
    // No callback from now on. The abort goes out in the next process_sdo_() pass: this may run from a callback or a
    // log handler in the middle of a transmission, which owns the client's TX buffer.
    this->sdo_active_.callback = nullptr;
    this->sdo_cancel_pending_ = true;
    this->enable_loop();
  }
}

CO_SDO_return_t CANopenComponent::start_sdo_() {
  CO_SDOclient_t *cli = this->co_->SDOclient;
  SDORequest &r = this->sdo_active_;
  uint8_t node = r.node_id;
  CO_SDO_return_t ret = CO_SDOclient_setup(cli, 0x600 + node, 0x580 + node, node);
  if (ret != CO_SDO_RT_ok_communicationEnd)
    return ret;
  this->sdo_len_ = 0;
  this->sdo_rx_value_.fill(0);  // finish_sdo_() decodes all 4 bytes
  switch (r.kind) {
    case SDOKind::SDO_KIND_READ:
    case SDOKind::SDO_KIND_READ_DATA:
      return CO_SDOclientUploadInitiate(cli, r.index, r.sub, this->response_timeout_ms_, false);
    case SDOKind::SDO_KIND_WRITE: {
      uint8_t buf[4];
      put_le32(buf, r.value);
      ret = CO_SDOclientDownloadInitiate(cli, r.index, r.sub, r.size, this->response_timeout_ms_, false);
      if (ret != CO_SDO_RT_ok_communicationEnd)
        return ret;
      this->sdo_len_ = CO_SDOclientDownloadBufWrite(cli, buf, r.size);
      return this->sdo_len_ == r.size ? CO_SDO_RT_ok_communicationEnd : CO_SDO_RT_wrongArguments;
    }
    case SDOKind::SDO_KIND_WRITE_DATA:
      ret = CO_SDOclientDownloadInitiate(cli, r.index, r.sub, r.len, this->response_timeout_ms_, false);
      if (ret != CO_SDO_RT_ok_communicationEnd)
        return ret;
      this->sdo_len_ = CO_SDOclientDownloadBufWrite(cli, r.data, r.len);
      return ret;
  }
  return CO_SDO_RT_wrongArguments;
}

void CANopenComponent::process_sdo_abort_(uint32_t dt_us) {
  if (this->sdo_abort_pending_) {
    CO_SDOclient_t *cli = this->co_->SDOclient;
    if (!cli->CANtxBuff->bufferFull) {
      // The CAN driver took it since the last pass. The guard starts now, whole: the time the driver took to accept it
      // doesn't count. Taken isn't sent: canbus doesn't tell when a frame is on the bus.
      this->sdo_abort_pending_ = false;
      this->sdo_abort_guard_us_ = ABORT_GUARD_US;
      return;
    }
    if (this->sdo_abort_guard_us_ <= dt_us) {
      // The bus refused it for a whole response timeout: drop it, so it doesn't hold up the queue. The server ends
      // the transfer at its own timeout.
      discard_pending_tx(cli->CANdevTx, cli->CANtxBuff);
      this->sdo_abort_pending_ = false;
      ESP_LOGW(TAG, "node 0x%02x: abort of a cancelled transfer not sent", cli->nodeIDOfTheSDOServer);
    }
  }
  this->sdo_abort_guard_us_ = this->sdo_abort_guard_us_ > dt_us ? this->sdo_abort_guard_us_ - dt_us : 0;
}

void CANopenComponent::process_sdo_(uint32_t dt_us) {
  this->process_sdo_abort_(dt_us);
  if (this->sdo_cancel_pending_) {
    // after process_sdo_abort_(): the abort's time starts with this pass
    this->sdo_cancel_pending_ = false;
    if (this->sdo_busy_)
      this->abort_sdo_();
  }
  if (!this->sdo_busy_) {
    // The guard holds the client's TX buffer while an abort is pending, then requests to every node: one guard covers
    // any number of cancelled transfers.
    if (this->sdo_queue_.empty() || this->sdo_abort_guard_us_ > 0)
      return;
    this->sdo_active_ = this->sdo_queue_.front();
    this->sdo_queue_.pop();
    this->sdo_busy_ = true;
    CO_SDO_return_t started = this->start_sdo_();
    if (started != CO_SDO_RT_ok_communicationEnd) {
      ESP_LOGD(TAG, "node 0x%02x, 0x%04x:%u: not started (CANopenNode %d)", this->sdo_active_.node_id,
               this->sdo_active_.index, this->sdo_active_.sub, static_cast<int>(started));
      this->finish_sdo_(SDOResult::SDO_RESULT_TRANSPORT_ERROR, 0);
      return;
    }
    dt_us = 0;
  }

  CO_SDOclient_t *cli = this->co_->SDOclient;
  SDORequest &r = this->sdo_active_;
  CO_SDO_abortCode_t abort = CO_SDO_AB_NONE;
  CO_SDO_return_t ret;
  if (r.kind == SDOKind::SDO_KIND_READ || r.kind == SDOKind::SDO_KIND_READ_DATA) {
    if (this->drain_upload_(dt_us, ret, abort))
      return;
  } else {
    if (r.kind == SDOKind::SDO_KIND_WRITE_DATA && this->sdo_len_ < r.len) {
      this->sdo_len_ += CO_SDOclientDownloadBufWrite(cli, r.data + this->sdo_len_, r.len - this->sdo_len_);
    }
    bool partial = r.kind == SDOKind::SDO_KIND_WRITE_DATA && this->sdo_len_ < r.len;
    ret = CO_SDOclientDownload(cli, dt_us, false, partial, &abort, nullptr, nullptr);
  }

  if (ret == CO_SDO_RT_ok_communicationEnd) {
    this->finish_sdo_(SDOResult::SDO_RESULT_SUCCESS, 0);
  } else if (ret == CO_SDO_RT_transmittBufferFull && cli->timeoutTimer >= cli->SDOtimeoutTime_us) {
    // CANopenNode doesn't time out while its frame waits to be sent
    this->finish_sdo_(SDOResult::SDO_RESULT_TRANSPORT_ERROR, 0);
  } else if (ret < 0) {
    // a server may abort with the timeout code too
    if (ret != CO_SDO_RT_endedWithServerAbort && abort == CO_SDO_AB_TIMEOUT) {
      this->finish_sdo_(SDOResult::SDO_RESULT_TIMEOUT, abort);
    } else if (ret == CO_SDO_RT_endedWithServerAbort || ret == CO_SDO_RT_endedWithClientAbort) {
      this->finish_sdo_(SDOResult::SDO_RESULT_ABORT, abort);
    } else {
      this->finish_sdo_(SDOResult::SDO_RESULT_TRANSPORT_ERROR, abort);
    }
  }
}

bool CANopenComponent::drain_upload_(uint32_t dt_us, CO_SDO_return_t &ret, CO_SDO_abortCode_t &abort) {
  CO_SDOclient_t *cli = this->co_->SDOclient;
  SDORequest &r = this->sdo_active_;
  const bool value_read = r.kind == SDOKind::SDO_KIND_READ;
  uint8_t *dst = value_read ? this->sdo_rx_value_.data() : r.data;
  const size_t cap = value_read ? this->sdo_rx_value_.size() : r.len;
  // The fifo holds one segment, so a read past capacity ends before the next segment is requested.
  ret = CO_SDOclientUpload(cli, dt_us, false, &abort, nullptr, nullptr, nullptr);
  while (ret >= 0) {
    uint8_t buf[8];
    size_t n;
    while ((n = CO_SDOclientUploadBufRead(cli, buf, sizeof(buf))) > 0) {
      if (this->sdo_len_ < cap)
        std::memcpy(dst + this->sdo_len_, buf, std::min(n, cap - this->sdo_len_));
      this->sdo_len_ += n;
    }
    if (this->sdo_len_ > cap) {
      abort = value_read ? CO_SDO_AB_DATA_LONG : CO_SDO_AB_OUT_OF_MEM;
      CO_SDOclientUpload(cli, 0, true, &abort, nullptr, nullptr, nullptr);
      // not abort: it is empty when the last frame already ended the transfer
      if (value_read) {
        this->finish_sdo_(SDOResult::SDO_RESULT_SIZE_MISMATCH, 0);
      } else {
        this->finish_sdo_(SDOResult::SDO_RESULT_ABORT, CO_SDO_AB_OUT_OF_MEM);
      }
      return true;
    }
    if (ret != CO_SDO_RT_uploadDataBufferFull && ret != CO_SDO_RT_waitingLocalTransfer)
      break;
    ret = CO_SDOclientUpload(cli, 0, false, &abort, nullptr, nullptr, nullptr);
  }
  return false;
}

void CANopenComponent::finish_sdo_(SDOResult result, uint32_t abort_code) {
  CO_SDOclient_t *cli = this->co_->SDOclient;
  CO_SDOclientClose(cli);
  // drop an unsent frame; one the controller took can't be recalled (see types.h)
  if (result != SDOResult::SDO_RESULT_SUCCESS)
    discard_pending_tx(cli->CANdevTx, cli->CANtxBuff);
  SDORequest r = this->sdo_active_;
  this->sdo_busy_ = false;

  SDOReply reply{result, abort_code, 0, 0, nullptr, 0};
  if (result == SDOResult::SDO_RESULT_SUCCESS) {
    if (r.kind == SDOKind::SDO_KIND_READ) {
      size_t len = this->sdo_len_;
      if (len == 0 || (r.size != 0 && len != r.size)) {
        reply.result = SDOResult::SDO_RESULT_SIZE_MISMATCH;
      } else {
        reply.value = encode_value_le<uint32_t>(this->sdo_rx_value_.data());
      }
      reply.size = len;
    } else if (r.kind == SDOKind::SDO_KIND_READ_DATA) {
      reply.data = r.data;
      reply.len = this->sdo_len_;
    } else if (r.kind == SDOKind::SDO_KIND_WRITE) {
      reply.value = r.value;
      reply.size = r.size;
    }
  }
  // the slot is free now, so the callback may queue the next request
  if (r.callback != nullptr)
    r.callback(r.context, reply);
  if (reply.result != SDOResult::SDO_RESULT_SUCCESS) {
    ESP_LOGW(TAG, "node 0x%02x, 0x%04x:%u: %s (abort 0x%08" PRIx32 ")", r.node_id, r.index, r.sub,
             LOG_STR_ARG(sdo_result_to_string(reply.result)), abort_code);
  }
}

void CANopenComponent::abort_sdo_() {
  CO_SDOclient_t *cli = this->co_->SDOclient;
  // The request ends before anything that may log (the abort's transmission too): a log handler may queue or cancel
  // requests. sdo_active_ stays as it is until the next request starts.
  const SDORequest &r = this->sdo_active_;
  this->sdo_busy_ = false;
  // Discard only this client's unsent frame before replacing it with an abort.
  // The driver and the SDO client run on the same main-loop context.
  discard_pending_tx(cli->CANdevTx, cli->CANtxBuff);
  CO_SDO_abortCode_t abort = CO_SDO_AB_GENERAL;
  if (r.kind == SDOKind::SDO_KIND_READ || r.kind == SDOKind::SDO_KIND_READ_DATA) {
    CO_SDOclientUpload(cli, 0, true, &abort, nullptr, nullptr, nullptr);
  } else {
    CO_SDOclientDownload(cli, 0, true, false, &abort, nullptr, nullptr);
  }
  CO_SDOclientClose(cli);
  // pending until the next pass, even if the driver took it at once (see process_sdo_abort_()); until then the guard
  // is the time left to send it
  this->sdo_abort_pending_ = true;
  this->sdo_abort_guard_us_ = this->response_timeout_ms_ * 1000u;
  ESP_LOGD(TAG, "node 0x%02x, 0x%04x:%u: cancelled", r.node_id, r.index, r.sub);
}

}  // namespace esphome::canopen

#endif  // USE_CANOPEN_SDO_CLIENT
