#pragma once
#include "coap_client_component.h"
#ifdef USE_ESP32
#include <tuple>
#include "esphome/components/json/json_util.h"
#include "esphome/core/automation.h"

namespace esphome::coap_client {

class CoapResponseStatistics {
 public:
  size_t content_length;
  uint16_t status_code;
  uint32_t duration_ms;
  uint32_t start_ms;

  void clean() {
    this->content_length = 0;
    this->status_code = 0;
    this->duration_ms = 0;
    this->start_ms = 0;
  }
};

// Send Action
template<typename... Ts> class CoapClientSendAction : public Action<Ts...> {
 public:
  CoapClientSendAction(CoapClientComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, request_name)
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(std::string, method)
  TEMPLATABLE_VALUE(std::string, media_type)
  TEMPLATABLE_VALUE(std::string, payload)
  TEMPLATABLE_VALUE(bool, capture_response)
  TEMPLATABLE_VALUE(size_t, max_response_buffer_size)
  TEMPLATABLE_VALUE(bool, observe)
  TEMPLATABLE_VALUE(uint32_t, response_timeout)

  void play(Ts... x) override {
    // initialize
    this->capture_response_value_ = this->capture_response_.value(x...);
    this->max_response_buffer_size_value_ = this->max_response_buffer_size_.value(x...);
    if (this->request_name_.has_value()) {
      this->request_name_value_ = this->request_name_.value(x...);
    } else {
      this->request_name_value_ = std::to_string(esphome::micros());
    }
    this->response_stats_->clean();
    this->response_stats_->start_ms = esphome::millis();
    this->response_payload_ = "";
    this->captured_args_ = std::make_tuple(x...);
    // payload
    std::string payload;
    if (this->payload_.has_value()) {
      payload = this->payload_.value(x...);
    }
    if (!this->json_.empty()) {
      this->media_type_ = "APPLICATION_JSON";
      auto f = std::bind(&CoapClientSendAction<Ts...>::encode_json_, this, x..., std::placeholders::_1);
      payload = json::build_json(f);
    }
    if (this->json_func_ != nullptr) {
      this->media_type_ = "APPLICATION_JSON";
      auto f = std::bind(&CoapClientSendAction<Ts...>::encode_json_func_, this, x..., std::placeholders::_1);
      payload = json::build_json(f);
    }
    // tx_request
    std::unique_ptr<CoapClientRequestData> tx_request = std::make_unique<CoapClientRequestData>();
    tx_request->name = this->request_name_value_;
    tx_request->method = this->get_method_(this->method_.value(x...));
    tx_request->uri = this->url_.value(x...);
    tx_request->callback = CoapClientSendAction::callback;
    tx_request->callback_context = this;
    tx_request->media_type = this->get_media_type_(this->media_type_.value(x...));
    tx_request->payload = payload;
    tx_request->response_timeout = this->response_timeout_.value(x...);
    tx_request->observe = this->observe_.value(x...);
    this->parent_->process_request(std::move(tx_request));
  }

  static void callback(uint16_t response_code, const unsigned char *data, size_t len, size_t offset, size_t total,
                       void *context) {
    CoapClientSendAction<Ts...> *obj = (CoapClientSendAction<Ts...> *) context;
    obj->process_response(response_code, data, len, offset, total);
  }

  void process_response(uint16_t response_code, const unsigned char *data, size_t len, size_t offset, size_t total) {
    auto resp_stats = this->response_stats_;

    if (len == 0 && offset == 0 && total == 0 && (response_code < 200 || response_code > 299)) {
      // Error
      resp_stats->content_length = 0;
      resp_stats->status_code = response_code;
      resp_stats->duration_ms = esphome::millis() - resp_stats->start_ms;
      resp_stats->start_ms = esphome::millis();
      this->trigger_this_();
      return;
    }
    if (len > 0) {
      if (this->capture_response_value_) {
        size_t store_len = std::min(len, this->max_response_buffer_size_value_ - offset);
        if (store_len > 0) {
          this->assign_response_payload_(data, len, offset);
        }
      }
    }
    if (total > 0 && len + offset == total) {
      resp_stats->content_length = total;
      resp_stats->status_code = response_code;
      resp_stats->duration_ms = esphome::millis() - resp_stats->start_ms;
      resp_stats->start_ms = esphome::millis();
      this->trigger_this_();
    }
  }

  // JSON Functions and Properties
  void add_json(const char *key, TemplatableValue<std::string, Ts...> value) { this->json_.insert({key, value}); }
  void set_json(std::function<void(Ts..., JsonObject)> json_func) { this->json_func_ = json_func; }

  // Trigger
  Trigger<std::shared_ptr<CoapResponseStatistics>, std::string &, std::string &, Ts...> *get_success_trigger() const {
    return this->success_trigger_;
  }

 protected:
  void trigger_this_() {
    auto resp_stats = this->response_stats_;
    auto request_name = this->request_name_value_;
    auto captured_args = this->captured_args_;

    // response_payload will be "" if capture_response = false;
    std::string response_payload = this->response_payload_;
    std::apply(
        [this, &resp_stats, &request_name, &response_payload](Ts... captured_args_inner) {
          this->success_trigger_->trigger(resp_stats, request_name, response_payload, captured_args_inner...);
        },
        captured_args);
  }

  CoapClientComponent *parent_;
  bool capture_response_value_{false};
  size_t max_response_buffer_size_value_{1000};
  std::string request_name_value_{};
  std::string response_payload_ = "";
  std::tuple<Ts...> captured_args_;

  // Blocks, Qblocks don't work with this would need to use a char* buffer and populate with offset
  void assign_response_payload_(const unsigned char *data, size_t len, size_t offset) {
    this->response_payload_.assign(reinterpret_cast<const char *>(data), len);
  }

  CoapMethod get_method_(std::string const &method) {
    if (method == "GET") {
      return CoapMethod::GET;
    } else if (method == "POST") {
      return CoapMethod::POST;
    } else if (method == "PUT") {
      return CoapMethod::PUT;
    } else if (method == "DELETE") {
      return CoapMethod::DELETE;
    } else if (method == "FETCH") {
      return CoapMethod::FETCH;
    } else if (method == "PATCH") {
      return CoapMethod::PATCH;
    } else if (method == "IPATCH") {
      return CoapMethod::IPATCH;
    }
    return CoapMethod::EMPTY;
  }

  CoapMediaType get_media_type_(std::string const &media_type) {
    if (media_type == "TEXT_PLAIN") {
      return CoapMediaType::TEXT_PLAIN;
    } else if (media_type == "APPLICATION_JSON") {
      return CoapMediaType::APPLICATION_JSON;
    } else if (media_type == "APPLICATION_LINK_FORMAT") {
      return CoapMediaType::APPLICATION_LINK_FORMAT;
    } else if (media_type == "APPLICATION_XML") {
      return CoapMediaType::APPLICATION_XML;
    } else if (media_type == "APPLICATION_OCTET_STREAM") {
      return CoapMediaType::APPLICATION_OCTET_STREAM;
    } else if (media_type == "APPLICATION_RDF_XML") {
      return CoapMediaType::APPLICATION_RDF_XML;
    } else if (media_type == "APPLICATION_EXI") {
      return CoapMediaType::APPLICATION_EXI;
    } else if (media_type == "APPLICATION_CBOR") {
      return CoapMediaType::APPLICATION_CBOR;
    } else if (media_type == "APPLICATION_CWT") {
      return CoapMediaType::APPLICATION_CWT;
    }
    return CoapMediaType::TEXT_PLAIN;
  }

  const std::shared_ptr<CoapResponseStatistics> response_stats_ = std::make_shared<CoapResponseStatistics>();

  // JSON Functions and Properties
  void encode_json_(Ts... x, JsonObject root) {
    for (const auto &item : this->json_) {
      auto val = item.second;
      root[item.first] = val.value(x...);
    }
  }
  void encode_json_func_(Ts... x, JsonObject root) { this->json_func_(x..., root); }
  std::map<const char *, TemplatableValue<std::string, Ts...>> json_{};
  std::function<void(Ts..., JsonObject)> json_func_{nullptr};

  // Trigger
  Trigger<std::shared_ptr<CoapResponseStatistics>, std::string &, std::string &, Ts...> *success_trigger_ =
      new Trigger<std::shared_ptr<CoapResponseStatistics>, std::string &, std::string &, Ts...>();
};

// Request Action
template<typename... Ts> class CoapClientRequestAction : public Action<Ts...> {
 public:
  CoapClientRequestAction(CoapClientComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, request_name)
  TEMPLATABLE_VALUE(std::string, method)
  TEMPLATABLE_VALUE(bool, pause)

  void play(Ts... x) override {
    if (this->method_.value(x...) == "STOP") {
      this->parent_->stop_request(this->request_name_.value(x...), this->pause_.value(x...));
    } else if (this->method_.value(x...) == "RESUME") {
      this->parent_->resume_request(this->request_name_.value(x...));
    } else if (this->method_.value(x...) == "REMOVE") {
      this->parent_->remove_request(this->request_name_.value(x...));
    }
  }

 protected:
  CoapClientComponent *parent_;
};

}  // namespace esphome::coap_client
#endif
