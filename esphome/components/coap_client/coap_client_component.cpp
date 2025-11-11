#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#ifdef USE_COAP_CLIENT
#include "esphome/core/log.h"
#ifdef USE_OPENTHREAD
#include "esphome/components/openthread/openthread.h"
#endif
#include "coap_client_component.h"

namespace esphome::coap_client_component {

static const char *TAG = "coap";

// CoapClientComponent
CoapClientComponent *global_coap_client = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

CoapClientComponent::CoapClientComponent() {
  global_coap_client = this;
  // Pre-allocate shared write buffer
}

void CoapClientComponent::setup() {
  /* Initialize libcoap library */
  this->request_queue_ = xQueueCreate(5, sizeof(CoapClientRequestData));
  if (this->request_queue_ == nullptr) {
    ESP_LOGE(TAG, "Setup of Request Queue failed");
    this->mark_failed();
    return;
  }
  coap_startup();
  xTaskCreate(
      [](void *arg) {
        CoapClientComponent *obj = (CoapClientComponent *) arg;
        obj->main_();
        coap_cleanup();
        vTaskDelete(nullptr);
        obj->toredown_ = true;
      },
      "CoapClientMain", 8 * 1024, this, 5, nullptr);
}

bool CoapClientComponent::teardown() {
  this->main_looping_ = false;
  return this->toredown_;
}

void CoapClientComponent::dump_config() { ESP_LOGCONFIG(TAG, "CoapClientComponent"); }

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
  void *context = this->response_callback_context_;
  coap_pdu_code_t rcvd_code = coap_pdu_get_code(received);
  if (COAP_RESPONSE_CLASS(rcvd_code) == 2) {
    if (coap_get_data_large(received, &data_len, &data, &offset, &total)) {
      this->response_callback_(data, data_len, offset, total, context);
    }
  } else {
    ESP_LOGE(TAG, "CoAP Response Code: %d.%02d", (rcvd_code >> 5), rcvd_code & 0x1F);
    data = nullptr;
    data_len = 0;
    offset = 0;
    total = 0;
    this->response_callback_(data, data_len, offset, total, context);
  }
  return COAP_RESPONSE_OK;
}

void CoapClientComponent::get(
    std::string uri,
    std::function<void(const unsigned char *data, size_t data_len, size_t offset, size_t total, void *context)>
        callback,
    void *callback_context, size_t max_block_size) {
  CoapClientRequestData send_request_data;
  send_request_data.uri = uri;
  send_request_data.callback = callback;
  send_request_data.callback_context = callback_context;
  send_request_data.max_block_size = max_block_size;
  BaseType_t sent_status;
  sent_status = xQueueSend(this->request_queue_, (void *) &send_request_data, pdMS_TO_TICKS(1000));
  if (sent_status != pdPASS) {
    // ESP_LOGI(TAG, "Sent the request_data to the queue %d", uxQueueMessagesWaiting(this->request_queue_));
    //} else {
    ESP_LOGE(TAG, "Failed to send the request_data to the queue %d", sent_status);
  }
}

void CoapClientComponent::main_() {
  coap_context_t *context = nullptr;
  coap_address_t dst_addr;
  coap_addr_info_t *info_list = nullptr;
  coap_optlist_t *opt_list = nullptr;
  coap_proto_t proto;
  coap_pdu_t *request = nullptr;
  coap_session_t *session = nullptr;
  coap_uri_t uri;

  CoapClientRequestData receive_request_data;
  BaseType_t receive_status;
  unsigned char token[8];
  size_t token_length;
  unsigned char uri_path[this->uri_path_buffer_size_];

  auto cleanup = [](void *self, coap_context_t *&context, coap_optlist_t *&opt_list, coap_session_t *&session) {
    // ESP_LOGD(TAG, "Coap cleanup");
    if (context) {
      coap_free_context(context);
    }
    if (opt_list) {
      coap_delete_optlist(opt_list);
      opt_list = nullptr;
    }
    if (session) {
      coap_session_release(session);
    }
    esphome::coap_client_component::CoapClientComponent *obj =
        (esphome::coap_client_component::CoapClientComponent *) self;
    if (obj->response_callback_) {
      obj->response_callback_(nullptr, 0, 0, 0, obj->response_callback_context_);
      obj->response_callback_ = nullptr;
      obj->response_callback_context_ = nullptr;
    }
  };

  // ESP_LOGD(TAG, "Begin coap main loop");
  for (; this->main_looping_; cleanup(this, context, opt_list, session)) {
    ESP_LOGD(TAG, "Top of main loop");
    receive_status = xQueueReceive(this->request_queue_, &receive_request_data, portMAX_DELAY);
    ESP_LOGD(TAG, "recieve_status: %d num: %d", receive_status, uxQueueMessagesWaiting(this->request_queue_));

    if (receive_status == pdPASS) {
      std::string request_uri = receive_request_data.uri;
      size_t max_block_size = receive_request_data.max_block_size;
      this->response_callback_context_ = receive_request_data.callback_context;
      this->response_callback_ = receive_request_data.callback;

      ESP_LOGD(TAG, "Process the receive_request_data from queue");
      context = coap_new_context(NULL);
      if (!context) {
        ESP_LOGE(TAG, "coap_new_context() failed");
        continue;
      }
      coap_context_set_block_mode(context, COAP_BLOCK_USE_LIBCOAP);
      coap_register_response_handler(context, CoapClientComponent::response_handler);
      coap_context_set_max_block_size(context, max_block_size);

      if (coap_split_uri((const uint8_t *) request_uri.c_str(), request_uri.length(), &uri) == -1) {
        ESP_LOGE(TAG, "Error coap_split_uri %s", request_uri.c_str());
        continue;
      }
      ESP_LOGD(TAG, "  Scheme: %d", uri.scheme);
      ESP_LOGD(TAG, "  Host: %.*s (length %zu)", (int) uri.host.length, uri.host.s, uri.host.length);
      ESP_LOGD(TAG, "  Port: %u", uri.port);  // Use %u for uint16_t
      ESP_LOGD(TAG, "  Path: %.*s (length %zu)\n", (int) uri.path.length, uri.path.s, uri.path.length);

      ESP_LOGD(TAG, "Create CoAP client request options");

      info_list = coap_resolve_address_info(&uri.host, uri.port, uri.port, uri.port, uri.port, 0, 1 << uri.scheme,
                                            COAP_RESOLVE_TYPE_REMOTE);
      if (info_list == NULL) {
        ESP_LOGE(TAG, "Error coap_resolve_address_info %s", request_uri.c_str());
        continue;
      }
      proto = info_list->proto;
      memcpy(&dst_addr, &info_list->addr, sizeof(dst_addr));
      coap_free_address_info(info_list);

      if (coap_uri_into_options(&uri, &dst_addr, &opt_list, 1, uri_path, sizeof(uri_path)) < 0) {
        ESP_LOGE(TAG, "Failed to create options for URI %s", request_uri.c_str());
        continue;
      }

      ESP_LOGD(TAG, "Create CoAP client session");
      session = coap_new_client_session(context, NULL, &dst_addr, proto);
      if (!session) {
        ESP_LOGE(TAG, "coap_new_client_session() failed");
        continue;
      }

      ESP_LOGD(TAG, "Create CoAP client request");
      request =
          coap_new_pdu(coap_is_mcast(&dst_addr) ? COAP_MESSAGE_NON : COAP_MESSAGE_CON, COAP_REQUEST_CODE_GET, session);
      if (!request) {
        ESP_LOGE(TAG, "coap_new_pdu() failed");
        continue;
      }

      ESP_LOGD(TAG, "Create CoAP client token");
      coap_session_new_token(session, &token_length, token);
      coap_add_token(request, token_length, token);
      coap_add_optlist_pdu(request, &opt_list);

      ESP_LOGD(TAG, "Send the request via CoAP");
      coap_send(session, request);

      // coap_io_process returns -1 on error.
      // coap_io_pending returns 1 if there's ongoing I/O (transfer in process), 0 if done.
      int result = 0;
      while (coap_io_pending(context) > 0) {
        // Wait for up to request_timeout_ milliseconds for I/O to happen.
        // The return value is the time spent in the function in milliseconds.
        result = coap_io_process(context, this->request_timeout_);
        if (result < 0) {
          ESP_LOGE(TAG, "CoAP I/O error! Exiting loop.");
          break;
        }
      }
      ESP_LOGD(TAG, "CoAP Request Processed");
    }
  }
}

}  // namespace esphome::coap_client_component
#endif
