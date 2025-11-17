/* Component Built from CoAP client Example

   The source example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
   Todo: subscriptions, use loop when there are subscriptions to make a request
*/
#include "esphome/core/defines.h"
#ifdef USE_COAP_CLIENT
#include "esphome/core/log.h"
#include "coap_client_component.h"

namespace esphome::coap {

static const char *TAG = "coap";

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
  this->main_looping_ = false;
  xTaskAbortDelay(this->main_task_handle_);
  return this->torndown_;
}

void CoapClientComponent::update() {
  if (this->is_ready() && this->tx_requests.size() > 0) {
    // TBD clean up to prevent leak
    /*
    auto it = this->tx_requests.begin();
    while (it != this->tx_requests.end()) {
        //remove requests that were never finished
        if (!it->second->subscribed and (micros() - it->second->timestamp > 500000)) { //5 minutes
            it = this->tx_requests.erase(it); // Erase the element and get the next valid iterator
        } else if (it->second->subscribed and (micros() - it->second->timestamp > 500000)) { //5 minutes
          ESP_LOGE(TAG, "Subscription to %s is stale", it->second->uri.c_str());
            ++it;
            it = this->tx_requests.erase(it); // Erase the element and get the next valid iterator
        } else {
            ++it; // Move to the next element if not erased
        }
    }
    */
  }
}

void CoapClientComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "CoAP Client:\n"
                "  Max Block Size: %d\n"
                "  Request Timeout: %ums\n"
                "  Ack Timeout: %ums\n"
                "  Max Retransmit: %d",
                this->max_block_size_, this->request_timeout_, this->ack_timeout_, this->max_retransmit_);
#ifdef CONFIG_COAP_OSCORE_SUPPORT
  ESP_LOGCONFIG(TAG, "  OSCore Conf Provided");
#endif
#ifdef CONFIG_COAP_MBEDTLS_PSK
  ESP_LOGCONFIG(TAG, "  MBDEDTLS PSK Provided");
#endif
#ifdef CONFIG_COAP_MBEDTLS_PKI
  ESP_LOGCONFIG(TAG, "  MBDEDTLS PKI Provided");
#endif
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
  coap_pdu_code_t rcvd_code = coap_pdu_get_code(received);
  uint16_t rcode = (((rcvd_code >> 5) & 0x07) * 100) + (rcvd_code & 0x1F);
  coap_bin_const_t pdu_token = coap_pdu_get_token(received);
  std::string token_str(reinterpret_cast<const char *>(pdu_token.s), pdu_token.length);
  if (!this->tx_requests.count(token_str)) {
    ESP_LOGE(TAG, "Unable to find CoAP Client Request");
    return COAP_RESPONSE_FAIL;
  }
  CoapClientRequestData &tx_request = *this->tx_requests[token_str];

  if (COAP_RESPONSE_CLASS(rcvd_code) == 2) {
    tx_request.timestamp = micros();
    if (tx_request.subscribe && !tx_request.subscribed) {
      tx_request.subscribed = true;
    }
    if (coap_get_data_large(received, &data_len, &data, &offset, &total)) {
      tx_request.callback(rcode, data, data_len, offset, total, tx_request.callback_context);
      if (data_len + offset == total && !tx_request.subscribe) {
        this->tx_requests.erase(token_str);
      }
    }
  } else {
    ESP_LOGE(TAG, "CoAP Response Code: %d.%02d", (rcvd_code >> 5), rcvd_code & 0x1F);
    data = nullptr;
    data_len = 0;
    offset = 0;
    total = 0;
    tx_request.callback(rcode, data, data_len, offset, total, tx_request.callback_context);
    this->tx_requests.erase(token_str);
  }
  return COAP_RESPONSE_OK;
}

void CoapClientComponent::get(std::string uri,
                              std::function<void(uint16_t response_code, const unsigned char *data, size_t data_len,
                                                 size_t offset, size_t total, void *context)>
                                  callback,
                              void *callback_context) {
  CoapClientRequestData tx_request = {
      .method = CoapMethod::GET,
      .uri = uri,
      .callback = callback,
      .callback_context = callback_context,
  };
  this->process_request(tx_request);
}

void CoapClientComponent::post(std::string uri,
                               std::function<void(uint16_t response_code, const unsigned char *data, size_t data_len,
                                                  size_t offset, size_t total, void *context)>
                                   callback,
                               void *callback_context, std::string payload, CoapMediaType media_type) {
  CoapClientRequestData tx_request = {.method = CoapMethod::POST,
                                      .uri = uri,
                                      .callback = callback,
                                      .callback_context = callback_context,
                                      .media_type = media_type,
                                      .payload = payload};
  this->process_request(tx_request);
}

void CoapClientComponent::process_request(CoapClientRequestData &tx_request) {
  ESP_LOGD(TAG, "%s %s", coap_method_to_string(tx_request.method), tx_request.uri.c_str());
  if (tx_request.payload.length() > 0) {
    ESP_LOGD(TAG, "payload media_type %s, payload starts with %.*s", coap_media_type_to_string(tx_request.media_type),
             std::min(10, (int) tx_request.payload.length()), tx_request.payload.c_str());
  }
  uint32_t dtime = micros();
  this->tx_request_storage_.emplace(dtime, tx_request);
  BaseType_t sent_status;
  sent_status = xQueueSend(this->request_queue_, (void *) &dtime, pdMS_TO_TICKS(1000));
  if (sent_status != pdPASS) {
    ESP_LOGE(TAG, "Failed to send the request_data to the queue %d", sent_status);
  }
}

void CoapClientComponent::main_() {
  this->main_task_handle_ = xTaskGetCurrentTaskHandle();
  coap_context_t *ctx = nullptr;
  coap_address_t dst_addr;
  coap_addr_info_t *info_list = nullptr;
  coap_optlist_t *opt_list = nullptr;
  coap_oscore_conf_t *oscore_conf = nullptr;
#ifdef CONFIG_COAP_OSCORE_SUPPORT
  coap_str_const_t osc_conf = {
      .s = this->oscore_conf_str_.c_str(),
      .length = this->oscore_conf_str_.length(),
  } oscore_conf = coap_new_oscore_conf(osc_conf, NULL, NULL, 0);
#endif
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

  auto cleanup = [](void *self, coap_optlist_t *&opt_list, coap_session_t *&session,
                    CoapClientRequestData *tx_request) {
    // ESP_LOGV(TAG, "Coap cleanup");
    if (opt_list) {
      coap_delete_optlist(opt_list);
      opt_list = nullptr;
    }
    if (session) {
      coap_session_release(session);
    }
    if (tx_request) {
      tx_request->callback(0, nullptr, 0, 0, 0, tx_request->callback_context);
      tx_request = nullptr;
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
    for (; this->main_looping_; cleanup(this, opt_list, session, tx_request)) {
      ESP_LOGV(TAG, "Top of main loop");
      receive_status = xQueueReceive(this->request_queue_, &dtime, portMAX_DELAY);
      ESP_LOGV(TAG, "recieve_status: %d num: %d", receive_status, uxQueueMessagesWaiting(this->request_queue_));
      // Request recieved from Queue
      if (receive_status == pdPASS) {
        uint32_t wait_ms = this->request_timeout_;
        // A subscribe and subscribed request is checking to see if any follow on
        // requests have happened.
        tx_request = &this->tx_request_storage_[dtime];
        if (!(tx_request->subscribe && tx_request->subscribed)) {
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
          session = this->get_session_(ctx, &dst_addr, &uri, proto, oscore_conf);
          if (!session) {
            ESP_LOGE(TAG, "create coap session failed");
            continue;
          }

          // Create CoAP Request
          request = coap_new_pdu(coap_is_mcast(&dst_addr) ? COAP_MESSAGE_NON : COAP_MESSAGE_CON,
                                 (coap_pdu_code_t) tx_request->method, session);
          if (!request) {
            ESP_LOGE(TAG, "coap_new_pdu() failed");
            continue;
          }

          // Create CoAP token
          if (tx_request->token.length() == 0) {
            coap_session_new_token(session, &token_length, token);
            tx_request->token.assign(reinterpret_cast<const char *>(token), token_length);
          }
          coap_add_token(request, tx_request->token.length(), (const uint8_t *) tx_request->token.c_str());

          // Subscribe
          if (tx_request->subscribe && !tx_request->subscribed) {
            u_char buf[4];
            coap_add_option(
                request, COAP_OPTION_OBSERVE,
                coap_encode_var_safe(buf, sizeof(buf), COAP_OBSERVE_ESTABLISH),  // COAP_OBSERVE_ESTABLISH is 0
                buf);
          }
          // Unsubscribe
          if (tx_request->subscribed && !tx_request->subscribe) {
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
          std::unique_ptr<CoapClientRequestData> utx_request_ptr = std::make_unique<CoapClientRequestData>();
          utx_request_ptr->subscribe = tx_request->subscribe;
          utx_request_ptr->subscribed = tx_request->subscribed;
          utx_request_ptr->qblock = tx_request->qblock;
          utx_request_ptr->token = tx_request->token;
          utx_request_ptr->method = tx_request->method;
          utx_request_ptr->uri = tx_request->uri;
          utx_request_ptr->callback = tx_request->callback;
          utx_request_ptr->callback_context = tx_request->callback_context;
          utx_request_ptr->media_type = tx_request->media_type;
          utx_request_ptr->payload = tx_request->payload;
          this->tx_requests.emplace(tx_request->token, std::move(utx_request_ptr));
          this->tx_request_storage_.erase(dtime);
          coap_send(session, request);
        } else {
          wait_ms = COAP_IO_NO_WAIT;
        }
        tx_request = nullptr;
        // coap_io_process returns -1 on error.
        // coap_io_pending returns 1 if there's ongoing I/O (transfer in process), 0 if done.
        int result = 0;
        while (1) {
          int pending = coap_io_pending(ctx);
          if (pending < 1) {
            ESP_LOGD(TAG, "No I/O Pending");
            break;
          }
          // Wait for up to request_timeout_ milliseconds for I/O to happen.
          // The return value is the time spent in the function in milliseconds.
          result = coap_io_process(ctx, wait_ms);
          if (result < 0) {
            ESP_LOGE(TAG, "CoAP I/O error! Exiting loop.");
            break;
          }
        }
        ESP_LOGV(TAG, "CoAP Request Processed");
      }
    }
    if (ctx) {
      coap_free_context(ctx);
    }
  }
}

#ifdef CONFIG_COAP_MBEDTLS_PKI
int CoapClientComponent::validate_cn_callback(const char *cn, const uint8_t *asn1_public_cert, size_t asn1_length,
                                              coap_session_t *session, unsigned depth, int validated, void *arg) {
  ESP_LOGD(TAG, "CN '%s' presented by server (%s)", cn, depth ? "CA" : "Certificate");
  return 1;
}
#endif

coap_session_t *CoapClientComponent::get_session_(coap_context_t *ctx, coap_address_t *dst_addr, coap_uri_t *uri,
                                                  coap_proto_t proto, coap_oscore_conf_t *oscore_conf) {
  coap_session_t *session = nullptr;
  // Secure
  if (uri->scheme == COAP_URI_SCHEME_COAPS || uri->scheme == COAP_URI_SCHEME_COAPS_TCP ||
      uri->scheme == COAP_URI_SCHEME_COAPS_WS) {
#ifndef CONFIG_MBEDTLS_TLS_CLIENT
    ESP_LOGE(TAG, "MbedTLS (D)TLS Client Mode not configured");
    return session;
#endif

#ifdef CONFIG_COAP_MBEDTLS_PSK
    // PSK first choice (if configured)
    session = coap_start_psk_session_(ctx, dst_addr, uri, proto);
#endif
    // PKI is second choice, defaults to anonymous if not defined CONFIG_COAP_MBEDTLS_PKI
    if (!session) {
      session = coap_start_pki_session_(ctx, dst_addr, uri, proto);
    }
  } else {
    // below is non secure (coap://)
#ifdef CONFIG_COAP_OSCORE_SUPPORT
    session = coap_new_client_session_oscore(ctx, NULL, dst_addr, proto, oscore_conf);
#else
    session = coap_new_client_session(ctx, NULL, dst_addr, proto);
#endif
  }
  if (session) {
    coap_session_set_ack_timeout(session, (coap_fixed_point_t) (this->ack_timeout_ / 1000.0));
    coap_session_set_max_retransmit(session, this->max_retransmit_);

#ifdef CONFIG_COAP_WEBSOCKETS
    if (proto == COAP_PROTO_WS || proto == COAP_PROTO_WSS) {
      coap_ws_set_host_request(session, &uri->host);
    }
#endif
  }
  return session;
}

#ifdef CONFIG_COAP_MBEDTLS_PSK
void CoapClientComponent::provision_psk_(coap_dtls_cpsk_t *dtls_psk, coap_uri_t *uri) {
  char client_sni[256];
  memset(client_sni, 0, sizeof(client_sni));
  memset(dtls_psk, 0, sizeof(coap_dtls_cpsk_t));
  dtls_psk->version = COAP_DTLS_CPSK_SETUP_VERSION;
  dtls_psk->validate_ih_call_back = NULL;
  dtls_psk->ih_call_back_arg = NULL;
  if (uri->host.length) {
    memcpy(client_sni, uri->host.s, std::min(uri->host.length, sizeof(client_sni) - 1));
  } else {
    memcpy(client_sni, "localhost", 9);
  }
  dtls_psk->client_sni = client_sni;
  dtls_psk->psk_info.identity.s = (const uint8_t *) this->psk_identity_.c_str();
  dtls_psk->psk_info.identity.length = this->psk_identity_.length();
  dtls_psk->psk_info.key.s = (const uint8_t *) this->psk_key_.c_str();
  dtls_psk->psk_info.key.length = this->psk_key_.length();
}

coap_session_t *CoapClientComponent::coap_start_psk_session_(coap_context_t *ctx, coap_address_t *dst_addr,
                                                             coap_uri_t *uri, coap_proto_t proto) {
  coap_dtls_cpsk_t dtls_psk;
  this->provision_psk_(&dtls_psk, uri);
#ifdef CONFIG_COAP_OSCORE_SUPPORT
  return coap_new_client_session_oscore_psk(ctx, NULL, dst_addr, proto, &dtls_psk, oscore_conf);
#else
  return coap_new_client_session_psk2(ctx, NULL, dst_addr, proto, &dtls_psk);
#endif
}
#endif

void CoapClientComponent::provision_pki_(coap_dtls_pki_t *dtls_pki, coap_uri_t *uri) {
  char client_sni[256];
  memset(client_sni, 0, sizeof(client_sni));
  memset(dtls_pki, 0, sizeof(coap_dtls_pki_t));
  dtls_pki->version = COAP_DTLS_PKI_SETUP_VERSION;
#ifdef CONFIG_COAP_MBEDTLS_PKI
  dtls_pki->verify_peer_cert = 1;
  dtls_pki->check_common_ca = 1;
  dtls_pki->allow_self_signed = 1;
  dtls_pki->allow_expired_certs = 1;
  dtls_pki->cert_chain_validation = 1;
  dtls_pki->cert_chain_verify_depth = 2;
  dtls_pki->check_cert_revocation = 1;
  dtls_pki->allow_no_crl = 1;
  dtls_pki->allow_expired_crl = 1;
  dtls_pki->allow_bad_md_hash = 1;
  dtls_pki->allow_short_rsa_length = 1;
  dtls_pki->validate_cn_call_back = this->validate_cn_callback;
  dtls_pki->cn_call_back_arg = NULL;
  dtls_pki->validate_sni_call_back = NULL;
  dtls_pki->sni_call_back_arg = NULL;
#endif
  if (uri->host.length) {
    memcpy(client_sni, uri->host.s, std::min(uri->host.length, sizeof(client_sni)));
  } else {
    memcpy(client_sni, "localhost", 9);
  }
  dtls_pki->client_sni = client_sni;
  dtls_pki->pki_key.key_type = COAP_PKI_KEY_PEM_BUF;

#ifdef CONFIG_COAP_MBEDTLS_PKI
  dtls_pki->pki_key.key.pem_buf.public_cert = this->client_crt_str_.c_str();
  dtls_pki->pki_key.key.pem_buf.public_cert_len = this->client_crt_str_.length();
  dtls_pki->pki_key.key.pem_buf.private_key = this->client_key_str_.c_str();
  dtls_pki->pki_key.key.pem_buf.private_key_len = this->client_key_str_.length();
  dtls_pki->pki_key.key.pem_buf.ca_cert = this->ca_pem_str_.c_str();
  dtls_pki->pki_key.key.pem_buf.ca_cert_len = this->ca_pem_str_.length();
#else
  dtls_pki->pki_key.key.pem.public_cert = NULL;
  dtls_pki->pki_key.key.pem.private_key = NULL;
  dtls_pki->pki_key.key.pem.ca_file = NULL;
#endif
}

coap_session_t *CoapClientComponent::coap_start_pki_session_(coap_context_t *ctx, coap_address_t *dst_addr,
                                                             coap_uri_t *uri, coap_proto_t proto) {
  coap_dtls_pki_t dtls_pki;
  this->provision_pki_(&dtls_pki, uri);
#ifdef CONFIG_COAP_OSCORE_SUPPORT
  return coap_new_client_session_oscore_pki(ctx, NULL, dst_addr, proto, &dtls_pki, oscore_conf);
#else
  return coap_new_client_session_pki(ctx, NULL, dst_addr, proto, &dtls_pki);
#endif
}

}  // namespace esphome::coap
#endif
