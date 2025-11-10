#include "esphome/core/defines.h"
#ifdef USE_COAP_CLIENT
#include "esphome/core/log.h"
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
  if (COAP_RESPONSE_CLASS(rcvd_code) > 2) {
    if (coap_get_data_large(received, &data_len, &data, &offset, &total)) {
      this->defer([this, data, data_len, offset, total, context]() {
        this->response_callback_(data, data_len, offset, total, context);
      });
      if (offset + data_len == total) {
        this->response_wait_ = false;
      }
    }
  } else {
    ESP_LOGE(TAG, "Coap Response Code: %d.%02d", (rcvd_code >> 5), rcvd_code & 0x1F);
    data = nullptr;
    data_len = 0;
    offset = 0;
    total = 0;
    this->defer([this, data, data_len, offset, total, context]() {
      this->response_callback_(data, data_len, offset, total, context);
    });
    this->response_wait_ = false;
  }
  return COAP_RESPONSE_OK;
}

void CoapClientComponent::get(
    std::string uri,
    std::function<void(const unsigned char *data, size_t data_len, size_t offset, size_t total, void *context)>
        callback,
    void *callback_context, size_t max_block_size) {
  CoapRequestData request_data;
  request_data.uri = uri;
  request_data.callback = callback;
  request_data.callback_context = callback_context;
  request_data.max_block_size = max_block_size;

  if (xQueueSend(request_queue_, (void *) &request_data, pdMS_TO_TICKS(1000)) != pdPASS) {
    ESP_LOGE(TAG, "Failed to send the request_data to the queue");
  } else {
    ESP_LOGI(TAG, "Send the request_data to the queue");
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

  CoapRequestData request_data;
  unsigned char token[8];
  size_t token_length;
  unsigned char uri_path[this->uri_path_buffer_size_];

  auto cleanup = [](coap_context_t *&context, coap_optlist_t *&opt_list, coap_session_t *&session) {
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
  };

  for (; this->main_looping_; cleanup(context, opt_list, session)) {
    if (xQueueReceive(this->request_queue_, &request_data, portMAX_DELAY) == pdPASS) {
      context = coap_new_context(NULL);
      if (!context) {
        ESP_LOGE(TAG, "coap_new_context() failed");
        continue;
      }
      coap_context_set_block_mode(context, COAP_BLOCK_USE_LIBCOAP);
      coap_register_response_handler(context, CoapClientComponent::response_handler);
      coap_context_set_max_block_size(context, request_data.max_block_size);

      if (coap_split_uri((const uint8_t *) request_data.uri.c_str(), request_data.uri.length(), &uri) == -1) {
        ESP_LOGE(TAG, "CoAP server uri %s error", request_data.uri.c_str());
        continue;
      }
      info_list = coap_resolve_address_info(&uri.host, uri.port, uri.port, uri.port, uri.port, 0, 1 << uri.scheme,
                                            COAP_RESOLVE_TYPE_REMOTE);
      if (info_list == NULL) {
        ESP_LOGE(TAG, "failed to resolve address %s", request_data.uri.c_str());
        continue;
      }
      proto = info_list->proto;
      memcpy(&dst_addr, &info_list->addr, sizeof(dst_addr));
      coap_free_address_info(info_list);

      if (coap_uri_into_options(&uri, &dst_addr, &opt_list, 1, uri_path, sizeof(uri_path)) < 0) {
        ESP_LOGE(TAG, "Failed to create options for URI %s", request_data.uri.c_str());
        continue;
      }

      session = coap_new_client_session(context, NULL, &dst_addr, proto);
      if (!session) {
        ESP_LOGE(TAG, "coap_new_client_session() failed");
        continue;
      }

      request =
          coap_new_pdu(coap_is_mcast(&dst_addr) ? COAP_MESSAGE_NON : COAP_MESSAGE_CON, COAP_REQUEST_CODE_GET, session);
      if (!request) {
        ESP_LOGE(TAG, "coap_new_pdu() failed");
        continue;
      }

      coap_session_new_token(session, &token_length, token);
      coap_add_token(request, token_length, token);
      coap_add_optlist_pdu(request, &opt_list);

      this->response_callback_context_ = request_data.callback_context;
      this->response_callback_ = request_data.callback;
      this->response_wait_ = true;
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
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

}  // namespace esphome::coap_client_component
#endif
