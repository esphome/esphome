/* Component Built from CoAP client Example

   The source example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "coap_client_component.h"
#ifdef USE_ESP32
#include "esphome/core/defines.h"
#ifdef USE_COAP_CLIENT
#include <format>
#include "esphome/core/log.h"

namespace esphome::coap_client {

static const char *TAG = "coap_client";

// CoapClientComponent
CoapClientComponent *global_coap_client = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

const char *coap_method_to_string(CoapMethod method) {
  switch (method) {
    case EMPTY:
      return "EMPTY";
    case GET:
      return "GET";
    case POST:
      return "POST";
    case PUT:
      return "PUT";
    case DELETE:
      return "DELETE";
    case FETCH:
      return "FETCH";
    case PATCH:
      return "PATCH";
    case IPATCH:
      return "IPATCH";
    default:
      return "UNKNOWN";
  }
}

const char *coap_media_type_to_string(CoapMediaType media_type) {
  switch (media_type) {
    case TEXT_PLAIN:
      return "TEXT_PLAIN";
    case APPLICATION_JSON:
      return "APPLICATION_JSON";
    case APPLICATION_LINK_FORMAT:
      return "APPLICATION_LINK_FORMAT";
    case APPLICATION_XML:
      return "APPLICATION_XML";
    case APPLICATION_OCTET_STREAM:
      return "APPLICATION_OCTET_STREAM";
    case APPLICATION_RDF_XML:
      return "APPLICATION_RDF_XML";
    case APPLICATION_EXI:
      return "APPLICATION_EXI";
    case APPLICATION_CBOR:
      return "APPLICATION_CBOR";
    case APPLICATION_CWT:
      return "APPLICATION_CWT";
    default:
      return "UNKNOWN";
  }
}

CoapClientComponent::CoapClientComponent() {
  global_coap_client = this;
  // Pre-allocate shared write buffer
}

void CoapClientComponent::setup() {
  // Initialize request queue
  this->request_queue_ = xQueueCreate(100, sizeof(uint32_t));
  if (this->request_queue_ == nullptr) {
    ESP_LOGE(TAG, "Setup of Request Queue failed");
    this->mark_failed();
    return;
  }
  // Initialize CoAP task
  coap_startup();
// Set up the CoAP logging
#if CONFIG_LOG_DYNAMIC_LEVEL_CONTROL
  ESP_LOGI(TAG, "Set CoAP Log Level to %d", CONFIG_LOG_DEFAULT_LEVEL);
  coap_set_log_level((coap_log_t) CONFIG_LOG_DEFAULT_LEVEL);
#endif

  xTaskCreate(
      [](void *arg) {
        CoapClientComponent *obj = (CoapClientComponent *) arg;
        obj->main_();
        coap_cleanup();
        vTaskDelete(nullptr);
        obj->torndown_ = true;
      },
      "CoapClientMain", 8 * 1024, this, 5, nullptr);
}

bool CoapClientComponent::teardown() {
  for (const auto &ptr : this->tx_requests_) {
    // Stop response or never response
    if (ptr->observe && ptr->observing) {
      ESP_LOGD(TAG, "Queue Request to terminate observation of Request: %s", ptr->name.c_str());
      ptr->observe = false;
      xQueueSend(this->request_queue_, (void *) &ptr->create_timestamp, pdMS_TO_TICKS(1000));
    }
  }
  uint8_t cnt = 100;
  while (this->tx_requests_.size() > 0 && cnt > 0) {
    delay(pdMS_TO_TICKS(this->request_timeout_));
    cnt--;
  }

  this->main_coap_loop_ = false;
  xTaskAbortDelay(this->main_task_handle_);
  return this->torndown_;
}

void CoapClientComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "CoAP Client:\n"
                "  Max Block Size: %d\n"
                "  Request Timeout: %ums\n"
                "  Ack Timeout: %ums\n"
                "  Max Retransmit: %d",
                this->max_block_size_, this->request_timeout_, this->ack_timeout_, this->max_retransmit_);
}

// static
coap_response_t CoapClientComponent::response_handler(coap_session_t *session, const coap_pdu_t *sent,
                                                      const coap_pdu_t *received, const coap_mid_t mid) {
  return global_coap_client->process_response(session, sent, received, mid);
}

coap_response_t CoapClientComponent::process_response(coap_session_t *session, const coap_pdu_t *sent,
                                                      const coap_pdu_t *received, const coap_mid_t mid) {
  const unsigned char *data = nullptr;
  size_t data_len;
  size_t offset;
  size_t total;
  std::lock_guard<std::mutex> lock(this->mutex_lock_);
  coap_pdu_code_t rcvd_code = coap_pdu_get_code(received);
  uint16_t rcode = (((rcvd_code >> 5) & 0x07) * 100) + (rcvd_code & 0x1F);
  coap_bin_const_t pdu_token = coap_pdu_get_token(received);
  ESP_LOGV(TAG, "Response pdu_token %d", coap_decode_var_bytes8(pdu_token.s, pdu_token.length));
  size_t tx_i = 0;
  bool is_found = false;
  for (const auto &ptr : this->tx_requests_) {
    if (coap_binary_equal(&pdu_token, ptr->pdu_token)) {
      is_found = true;
      break;
    }
    tx_i++;
  }
  CoapClientRequestData &tx_request = *this->tx_requests_[tx_i];
  if (!is_found) {
    // Silent fail
    ESP_LOGVV(TAG, "Unable to find CoAP Client Request");
    return COAP_RESPONSE_OK;
  }
  ESP_LOGV(TAG, "CoAP Client Request: %s", tx_request.name.c_str());
  tx_request.response_timestamp = micros();

  if (COAP_RESPONSE_CLASS(rcvd_code) == 2) {
    if (tx_request.observe && !tx_request.observing) {
      tx_request.observing = true;
    }
    if (coap_get_data_large(received, &data_len, &data, &offset, &total)) {
      tx_request.callback(rcode, data, data_len, offset, total, tx_request.callback_context);
      if (data_len + offset == total) {
        if (!tx_request.observe) {
          this->tx_requests_.erase(this->tx_requests_.begin() + tx_i);
        }
        this->inner_coap_loop_ = false;
      }
    }
  } else {
    ESP_LOGE(TAG, "CoAP Response Code: %d.%02d", (rcvd_code >> 5), rcvd_code & 0x1F);
    data = nullptr;
    data_len = 0;
    offset = 0;
    total = 0;
    tx_request.callback(rcode, data, data_len, offset, total, tx_request.callback_context);
    if (!tx_request.observe) {
      this->tx_requests_.erase(this->tx_requests_.begin() + tx_i);
    }
    this->inner_coap_loop_ = false;
  }
  return COAP_RESPONSE_OK;
}

void CoapClientComponent::get(std::string uri,
                              std::function<void(uint16_t response_code, const unsigned char *data, size_t data_len,
                                                 size_t offset, size_t total, void *context)>
                                  callback,
                              void *callback_context, uint32_t response_timeout) {
  CoapClientRequestData tx_request = {
      .method = CoapMethod::GET,
      .uri = uri,
      .callback = callback,
      .callback_context = callback_context,
      .response_timeout = response_timeout,
  };
  this->process_request(tx_request);
}

void CoapClientComponent::post(std::string uri,
                               std::function<void(uint16_t response_code, const unsigned char *data, size_t data_len,
                                                  size_t offset, size_t total, void *context)>
                                   callback,
                               void *callback_context, std::string payload, CoapMediaType media_type,
                               uint32_t response_timeout) {
  CoapClientRequestData tx_request = {
      .method = CoapMethod::POST,
      .uri = uri,
      .callback = callback,
      .callback_context = callback_context,
      .media_type = media_type,
      .payload = payload,
      .response_timeout = response_timeout,
  };
  this->process_request(tx_request);
}

void CoapClientComponent::process_request(CoapClientRequestData &tx_request) {
  ESP_LOGD(TAG, "%s %s", coap_method_to_string(tx_request.method), tx_request.uri.c_str());
  if (tx_request.payload.length() > 0) {
    ESP_LOGD(TAG, "payload media_type %s, payload starts with %.*s", coap_media_type_to_string(tx_request.media_type),
             std::min(10, (int) tx_request.payload.length()), tx_request.payload.c_str());
  }
  uint32_t dtime = micros();
  std::string name = tx_request.name.length() > 0 ? tx_request.name : std::format("{}", dtime);
  std::unique_ptr<CoapClientRequestData> utx_request_ptr = std::make_unique<CoapClientRequestData>();
  ESP_LOGV(TAG, "Name:'%s' oname:'%s', time:'%s'", name.c_str(), tx_request.name.c_str(),
           (std::format("{}", dtime)).c_str());
  utx_request_ptr->name = name;
  utx_request_ptr->create_timestamp = dtime;
  utx_request_ptr->response_timestamp = dtime;
  utx_request_ptr->observe = tx_request.observe;
  utx_request_ptr->observing = tx_request.observing;
  utx_request_ptr->qblock = tx_request.qblock;
  utx_request_ptr->method = tx_request.method;
  utx_request_ptr->uri = tx_request.uri;
  utx_request_ptr->callback = tx_request.callback;
  utx_request_ptr->callback_context = tx_request.callback_context;
  utx_request_ptr->media_type = tx_request.media_type;
  utx_request_ptr->payload = tx_request.payload;
  utx_request_ptr->response_timeout = tx_request.response_timeout;
  std::lock_guard<std::mutex> lock(this->mutex_lock_);
  this->tx_requests_.push_back(std::move(utx_request_ptr));
  BaseType_t sent_status;
  sent_status = xQueueSend(this->request_queue_, (void *) &dtime, pdMS_TO_TICKS(1000));
  if (sent_status != pdPASS) {
    ESP_LOGE(TAG, "Failed to send the request_data to the queue %d", sent_status);
  }
}

void CoapClientComponent::remove(std::string name) {
  for (const auto &ptr : this->tx_requests_) {
    if (ptr->observe && (name.length() == 0 || ptr->name == name)) {
      ESP_LOGD(TAG, "Queue Request to terminate observation of Request: %s", ptr->name.c_str());
      ptr->observe = false;
      xQueueSend(this->request_queue_, (void *) &ptr->create_timestamp, pdMS_TO_TICKS(1000));
    }
  }
}

void CoapClientComponent::main_() {
  this->main_task_handle_ = xTaskGetCurrentTaskHandle();
  coap_context_t *ctx = nullptr;
  coap_address_t dst_addr;
  coap_addr_info_t *info_list = nullptr;
  coap_optlist_t *opt_list = nullptr;
  coap_proto_t proto;
  coap_pdu_t *request = nullptr;
  coap_session_t *session = nullptr;
  coap_uri_t uri;

  CoapClientRequestData *tx_request = nullptr;
  uint32_t dtime;
  BaseType_t receive_status;
  unsigned char token[8];
  size_t token_length;
  unsigned char uri_path[this->uri_path_buffer_size_];

  auto cleanup = [](coap_optlist_t *&opt_list) {
    // ESP_LOGV(TAG, "Coap cleanup");
    if (opt_list) {
      coap_delete_optlist(opt_list);
      opt_list = nullptr;
    }
  };

  // New CoAP Context
  ctx = coap_new_context(NULL);
  if (!ctx) {
    ESP_LOGE(TAG, "coap_new_context() failed");
  } else {
    coap_context_set_block_mode(ctx, COAP_BLOCK_USE_LIBCOAP);
    coap_register_response_handler(ctx, CoapClientComponent::response_handler);
    coap_context_set_max_block_size(ctx, this->max_block_size_);

    // ESP_LOGV(TAG, "Begin coap main loop");
    for (; this->main_coap_loop_; cleanup(opt_list)) {
      ESP_LOGV(TAG, "Top of main loop");
      tx_request = nullptr;
      this->inner_coap_loop_ = true;
      receive_status = xQueueReceive(this->request_queue_, &dtime, 0);
      ESP_LOGV(TAG, "recieve_status: %d num: %d", receive_status, uxQueueMessagesWaiting(this->request_queue_));
      // Request recieved from Queue
      if (receive_status == pdPASS) {
        std::lock_guard<std::mutex> lock(this->mutex_lock_);
        for (const auto &ptr : this->tx_requests_) {
          if (dtime == ptr->create_timestamp) {
            tx_request = ptr.get();
          }
        };
        if (!tx_request) {
          ESP_LOGE(TAG, "unable to find queued request");
          continue;
        }
        ESP_LOGV(TAG, "Timestamp %d %s", dtime, tx_request->name.c_str());
        // Parse uri
        if (coap_split_uri((const uint8_t *) tx_request->uri.c_str(), tx_request->uri.length(), &uri) == -1) {
          ESP_LOGE(TAG, "Error coap_split_uri %s", tx_request->uri.c_str());
          continue;
        }
        ESP_LOGV(TAG, "  Scheme: %d", uri.scheme);
        ESP_LOGV(TAG, "  Host: %.*s (length %zu)", (int) uri.host.length, uri.host.s, uri.host.length);
        ESP_LOGV(TAG, "  Port: %u", uri.port);  // Use %u for uint16_t
        ESP_LOGV(TAG, "  Path: %.*s (length %zu)\n", (int) uri.path.length, uri.path.s, uri.path.length);

        // Get info_list (addr is destination, proto(col) is UDP, DTLS, TCP, TLS)
        info_list = coap_resolve_address_info(&uri.host, uri.port, uri.port, uri.port, uri.port, 0, 1 << uri.scheme,
                                              COAP_RESOLVE_TYPE_REMOTE);
        if (info_list == NULL) {
          ESP_LOGE(TAG, "Error coap_resolve_address_info %s", tx_request->uri.c_str());
          continue;
        }
        // Copy out proto(col) and destination
        proto = info_list->proto;
        memcpy(&dst_addr, &info_list->addr, sizeof(dst_addr));
        coap_free_address_info(info_list);

        // Build CoAP Options (header)
        if (coap_uri_into_options(&uri, &dst_addr, &opt_list, 1, uri_path, sizeof(uri_path)) < 0) {
          ESP_LOGE(TAG, "Failed to create options for URI %s", tx_request->uri.c_str());
          continue;
        }
        // Create CoAP Session
        if (tx_request->session) {
          session = tx_request->session;
        } else {
          session = this->get_session_(ctx, &dst_addr, &uri, proto);
          if (!session) {
            ESP_LOGE(TAG, "create coap session failed");
            continue;
          }
          coap_encode_var_safe8(token, 4, dtime);
          coap_session_init_token(session, 4, token);
          tx_request->session = session;
        }

        // Create CoAP Request
        request = coap_new_pdu(coap_is_mcast(&dst_addr) ? COAP_MESSAGE_NON : COAP_MESSAGE_CON,
                               (coap_pdu_code_t) tx_request->method, session);
        if (!request) {
          ESP_LOGE(TAG, "coap_new_pdu() failed");
          continue;
        }

        // Create CoAP token
        if (tx_request->pdu_token)  // observe, so session exists libcoap
        {
          ESP_LOGV(TAG, "Use pdu_token %d from %s",
                   coap_decode_var_bytes8(tx_request->pdu_token->s, tx_request->pdu_token->length),
                   tx_request->name.c_str());
          if (!coap_add_token(request, tx_request->pdu_token->length, tx_request->pdu_token->s)) {
            ESP_LOGE(TAG, "Unable to add token to request");
          }
        } else {
          coap_session_new_token(session, &token_length, token);
          ESP_LOGV(TAG, "New pdu_token %d for %s", coap_decode_var_bytes8(token, token_length),
                   tx_request->name.c_str());
          if (!coap_add_token(request, token_length, token)) {
            ESP_LOGE(TAG, "Unable to add token to request");
          }
          tx_request->set_pdu_token(token, token_length);
        }

        // Observe
        if (tx_request->observe && !tx_request->observing) {
          u_char buf[4];
          coap_add_option(
              request, COAP_OPTION_OBSERVE,
              coap_encode_var_safe(buf, sizeof(buf), COAP_OBSERVE_ESTABLISH),  // COAP_OBSERVE_ESTABLISH is 0
              buf);
        }
        // Unobserve
        if (!tx_request->observe && tx_request->observing) {
          u_char buf[4];
          coap_add_option(request, COAP_OPTION_OBSERVE,
                          coap_encode_var_safe(buf, sizeof(buf), COAP_OBSERVE_CANCEL),  // COAP_OBSERVE_ESTABLISH is 1
                          buf);
        }
        // Payload
        if (tx_request->payload.length() > 0) {
          u_char buf[4];
          coap_insert_optlist(&opt_list,
                              coap_new_optlist(COAP_OPTION_CONTENT_FORMAT,
                                               coap_encode_var_safe(buf, sizeof(buf), tx_request->media_type), buf));
          coap_add_data_large_request(session, request, tx_request->payload.length(),
                                      (const uint8_t *) tx_request->payload.c_str(), NULL, NULL);
        }

        // Add option list to CoAP request (it is added as part of header)
        coap_add_optlist_pdu(request, &opt_list);

        // Send CoAP request
        coap_send(session, request);
      }
      // coap_io_process returns -1 on error.
      // coap_io_pending returns 1 if there's ongoing I/O (transfer in process), 0 if done.
      int result = 0;
      uint32_t wait_ms = this->request_timeout_;
      while (this->inner_coap_loop_) {
        // Wait for up to request_timeout_ milliseconds for I/O to happen.
        // The return value is the time spent in the function in milliseconds.
        result = coap_io_process(ctx, wait_ms);
        if (result < 0) {
          ESP_LOGE(TAG, "CoAP I/O error! Exiting Inner Loop.");
          break;
        }
        if (result >= wait_ms) {
          ESP_LOGV(TAG, "No Processing Occurred Exiting Inner Loop");
          break;
        } else {
          // sample code did a speed up here
        }
      }
      yield();
      ESP_LOGV(TAG, "CoAP Request Processed");
      this->housekeeping_();
    }
    if (ctx) {
      coap_free_context(ctx);
    }
  }
}

void CoapClientComponent::housekeeping_() {
  if (this->tx_requests_.size() > 0) {
    // clean up to prevent leak
    CoapClientRequestData *tx_request = nullptr;
    size_t tx_i = 0;
    for (const auto &ptr : this->tx_requests_) {
      // Stop response or never response
      if (micros() - ptr->response_timestamp > 1000u * ptr->response_timeout) {
        ESP_LOGD(TAG, "Remove Request %s due to Response timeout of %ums", ptr->name.c_str(), ptr->response_timeout);
        tx_request = ptr.get();
        break;
      }
      tx_i++;
    }
    if (tx_request) {
      std::lock_guard<std::mutex> lock(this->mutex_lock_);
      if (tx_request->create_timestamp == tx_request->response_timestamp) {
        ESP_LOGV(TAG, "Send Error End to tx_request %s", tx_request->name.c_str());
        tx_request->callback(0, nullptr, 0, 0, 0, tx_request->callback_context);
      }
      this->tx_requests_.erase(this->tx_requests_.begin() + tx_i);
    }
  }
}

coap_session_t *CoapClientComponent::get_session_(coap_context_t *ctx, coap_address_t *dst_addr, coap_uri_t *uri,
                                                  coap_proto_t proto) {
  coap_session_t *session = nullptr;
  session = coap_new_client_session(ctx, NULL, dst_addr, proto);
  if (session) {
    coap_session_set_ack_timeout(session, (coap_fixed_point_t) (this->ack_timeout_ / 1000.0));
    coap_session_set_max_retransmit(session, this->max_retransmit_);
  }
  return session;
}

}  // namespace esphome::coap_client
#endif
#endif
