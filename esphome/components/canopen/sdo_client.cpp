// SDO client on CANopenNode's CO_SDOclient: one channel, a FIFO request queue, one callback per request.
#include "canopen.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen.sdo");

const LogString *sdo_result_to_string(SDOResult result) {
  switch (result) {
    case SDOResult::SDO_RESULT_SUCCESS:
      return LOG_STR("success");
    case SDOResult::SDO_RESULT_ABORT:
      return LOG_STR("abort");
    case SDOResult::SDO_RESULT_TIMEOUT:
      return LOG_STR("timeout");
    case SDOResult::SDO_RESULT_SIZE_MISMATCH:
      return LOG_STR("size mismatch");
    case SDOResult::SDO_RESULT_TRANSPORT_ERROR:
      return LOG_STR("transport error");
    case SDOResult::SDO_RESULT_REJECTED:
      return LOG_STR("rejected");
    default:
      return LOG_STR("unknown");
  }
}

void CANopenComponent::add_sdo_client_od_() {
  this->sdo_cli_record_ = {{
      {&this->sdo_cli_highest_sub_, 0, ODA_SDO_R, 1},
      {&this->sdo_cli_cob_tx_, 1, ODA_SDO_RW | ODA_MB, 4},
      {&this->sdo_cli_cob_rx_, 2, ODA_SDO_RW | ODA_MB, 4},
      {&this->sdo_cli_node_, 3, ODA_SDO_RW, 1},
  }};
  this->od_entries_[0] = OD_entry_t{0x1280, static_cast<uint8_t>(this->sdo_cli_record_.size()), ODT_REC,
                                    this->sdo_cli_record_.data(), nullptr};
  this->od_entries_[1] = OD_entry_t{0, 0, 0, nullptr, nullptr};
  this->od_.size = 1;
  this->od_.list = this->od_entries_.data();
}

void CANopenComponent::config_sdo_client_(CO_config_t &cfg) {
  cfg.CNT_SDO_CLI = 1;
  cfg.ENTRY_H1280 = &this->od_entries_[0];
}

bool CANopenComponent::init_sdo_client_() {
  uint32_t err_info = 0;
  CO_ReturnError_t err = CO_SDOclient_init(this->co_->SDOclient, &this->od_, &this->od_entries_[0], 0 /* no own node */,
                                           this->co_->CANmodule, this->co_->RX_IDX_SDO_CLI, this->co_->CANmodule,
                                           this->co_->TX_IDX_SDO_CLI, &err_info);
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

// unsigned or sign-extended, the range validate_sdo_value() checks for constants
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
      SDORequest{node_id, index, sub, SDOKind::SDO_KIND_READ, expected_size, 0, nullptr, 0, callback, context});
}

bool CANopenComponent::write_sdo(uint8_t node_id, uint16_t index, uint8_t sub, uint32_t value, uint8_t size,
                                 SDOCallback callback, void *context) {
  if ((size != 1 && size != 2 && size != 4) || !value_fits(value, size)) {
    reject(node_id, index, sub, callback, context);
    return false;
  }
  return this->queue_sdo_(
      SDORequest{node_id, index, sub, SDOKind::SDO_KIND_WRITE, size, value, nullptr, 0, callback, context});
}

bool CANopenComponent::read_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, uint8_t *data, size_t capacity,
                                     SDOCallback callback, void *context) {
  if (data == nullptr || capacity == 0) {
    reject(node_id, index, sub, callback, context);
    return false;
  }
  return this->queue_sdo_(
      SDORequest{node_id, index, sub, SDOKind::SDO_KIND_READ_DATA, 0, 0, data, capacity, callback, context});
}

bool CANopenComponent::write_sdo_data(uint8_t node_id, uint16_t index, uint8_t sub, const uint8_t *data, size_t len,
                                      SDOCallback callback, void *context) {
  if (data == nullptr || len == 0) {
    reject(node_id, index, sub, callback, context);
    return false;
  }
  // a WRITE_DATA request only reads it
  return this->queue_sdo_(SDORequest{node_id, index, sub, SDOKind::SDO_KIND_WRITE_DATA, 0, 0,
                                     const_cast<uint8_t *>(data), len, callback, context});
}

bool CANopenComponent::queue_sdo_(SDORequest &&request) {
  if (this->co_ == nullptr || this->is_failed() || request.node_id == 0 || request.node_id > 127 ||
      this->sdo_count_ == SDO_QUEUE_SIZE) {
    reject(request.node_id, request.index, request.sub, request.callback, request.context);
    return false;
  }
  this->sdo_queue_[(this->sdo_head_ + this->sdo_count_) % SDO_QUEUE_SIZE] = request;
  this->sdo_count_++;
  return true;
}

bool CANopenComponent::start_sdo_() {
  CO_SDOclient_t *cli = this->co_->SDOclient;
  SDORequest &r = this->sdo_active_;
  uint8_t node = r.node_id;
  if (CO_SDOclient_setup(cli, 0x600 + node, 0x580 + node, node) != CO_SDO_RT_ok_communicationEnd)
    return false;
  this->sdo_offset_ = 0;
  this->sdo_rx_len_ = 0;
  switch (r.kind) {
    case SDOKind::SDO_KIND_READ:
    case SDOKind::SDO_KIND_READ_DATA:
      return CO_SDOclientUploadInitiate(cli, r.index, r.sub, this->response_timeout_ms_, false) ==
             CO_SDO_RT_ok_communicationEnd;
    case SDOKind::SDO_KIND_WRITE: {
      uint8_t buf[4] = {(uint8_t) r.value, (uint8_t) (r.value >> 8), (uint8_t) (r.value >> 16),
                        (uint8_t) (r.value >> 24)};
      if (CO_SDOclientDownloadInitiate(cli, r.index, r.sub, r.size, this->response_timeout_ms_, false) !=
          CO_SDO_RT_ok_communicationEnd)
        return false;
      this->sdo_offset_ = CO_SDOclientDownloadBufWrite(cli, buf, r.size);
      return this->sdo_offset_ == r.size;
    }
    case SDOKind::SDO_KIND_WRITE_DATA:
      if (CO_SDOclientDownloadInitiate(cli, r.index, r.sub, r.len, this->response_timeout_ms_, false) !=
          CO_SDO_RT_ok_communicationEnd)
        return false;
      this->sdo_offset_ = CO_SDOclientDownloadBufWrite(cli, r.data, r.len);
      return true;
  }
  return false;
}

void CANopenComponent::process_sdo_(uint32_t dt_us) {
  if (!this->sdo_busy_) {
    if (this->sdo_count_ == 0)
      return;
    this->sdo_active_ = this->sdo_queue_[this->sdo_head_];
    this->sdo_head_ = (this->sdo_head_ + 1) % SDO_QUEUE_SIZE;
    this->sdo_count_--;
    this->sdo_busy_ = true;
    if (!this->start_sdo_()) {
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
    ret = CO_SDOclientUpload(cli, dt_us, false, &abort, nullptr, nullptr, nullptr);
    if (ret == CO_SDO_RT_uploadDataBufferFull || ret == CO_SDO_RT_ok_communicationEnd) {
      uint8_t *dst = this->sdo_rx_value_.data();
      size_t cap = this->sdo_rx_value_.size();
      size_t limit = SDO_MAX_VALUE_UPLOAD;
      if (r.kind == SDOKind::SDO_KIND_READ_DATA) {
        dst = r.data;
        cap = r.len;
        limit = cap;
      }
      uint8_t buf[64];
      size_t n;
      while ((n = CO_SDOclientUploadBufRead(cli, buf, sizeof(buf))) > 0) {
        if (this->sdo_rx_len_ < cap)
          std::memcpy(dst + this->sdo_rx_len_, buf, std::min(n, cap - this->sdo_rx_len_));
        this->sdo_rx_len_ += n;
      }
      if (this->sdo_rx_len_ > limit) {
        abort = CO_SDO_AB_OUT_OF_MEM;
        CO_SDOclientUpload(cli, 0, true, &abort, nullptr, nullptr, nullptr);
        // not abort: it comes back empty when the last frame ended the transfer, with nothing left to abort
        this->finish_sdo_(SDOResult::SDO_RESULT_ABORT, CO_SDO_AB_OUT_OF_MEM);
        return;
      }
    }
  } else {
    if (r.kind == SDOKind::SDO_KIND_WRITE_DATA && this->sdo_offset_ < r.len) {
      this->sdo_offset_ += CO_SDOclientDownloadBufWrite(cli, r.data + this->sdo_offset_, r.len - this->sdo_offset_);
    }
    bool partial = r.kind == SDOKind::SDO_KIND_WRITE_DATA && this->sdo_offset_ < r.len;
    ret = CO_SDOclientDownload(cli, dt_us, false, partial, &abort, nullptr, nullptr);
  }

  if (ret == CO_SDO_RT_ok_communicationEnd) {
    this->finish_sdo_(SDOResult::SDO_RESULT_SUCCESS, 0);
  } else if (ret == CO_SDO_RT_transmittBufferFull && cli->timeoutTimer >= cli->SDOtimeoutTime_us) {
    // CANopenNode doesn't time out while its frame waits to be sent: the queue would wait as long as the bus is stuck
    this->finish_sdo_(SDOResult::SDO_RESULT_TRANSPORT_ERROR, 0);
  } else if (ret < 0) {
    if (abort == CO_SDO_AB_TIMEOUT) {
      this->finish_sdo_(SDOResult::SDO_RESULT_TIMEOUT, abort);
    } else if (ret == CO_SDO_RT_endedWithServerAbort || ret == CO_SDO_RT_endedWithClientAbort) {
      this->finish_sdo_(SDOResult::SDO_RESULT_ABORT, abort);
    } else {
      this->finish_sdo_(SDOResult::SDO_RESULT_TRANSPORT_ERROR, abort);
    }
  }
}

void CANopenComponent::finish_sdo_(SDOResult result, uint32_t abort_code) {
  CO_SDOclient_t *cli = this->co_->SDOclient;
  CO_SDOclientClose(cli);
  // the caller is told it failed: its unsent frame must not reach the server later
  if (result != SDOResult::SDO_RESULT_SUCCESS)
    discard_pending_tx(cli->CANdevTx, cli->CANtxBuff);
  SDORequest r = this->sdo_active_;
  this->sdo_busy_ = false;

  SDOReply reply{result, abort_code, 0, 0, nullptr, 0};
  if (result == SDOResult::SDO_RESULT_SUCCESS) {
    if (r.kind == SDOKind::SDO_KIND_READ) {
      size_t len = this->sdo_rx_len_;
      if (len == 0 || len > 4 || (r.size != 0 && len != r.size)) {
        reply.result = SDOResult::SDO_RESULT_SIZE_MISMATCH;
      } else {
        for (size_t i = 0; i < len; i++)
          reply.value |= (uint32_t) this->sdo_rx_value_[i] << (8 * i);
      }
      reply.size = len;
    } else if (r.kind == SDOKind::SDO_KIND_READ_DATA) {
      reply.data = r.data;
      reply.len = this->sdo_rx_len_;
    } else if (r.kind == SDOKind::SDO_KIND_WRITE) {
      reply.value = r.value;
      reply.size = r.size;
    }
  }
  if (reply.result != SDOResult::SDO_RESULT_SUCCESS) {
    ESP_LOGW(TAG, "node 0x%02x, 0x%04x:%u: %s (abort 0x%08" PRIx32 ")", r.node_id, r.index, r.sub,
             LOG_STR_ARG(sdo_result_to_string(reply.result)), abort_code);
  }
  // the slot is free now, so the callback may queue the next request
  if (r.callback != nullptr)
    r.callback(r.context, reply);
}

}  // namespace esphome::canopen
