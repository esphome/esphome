#pragma once
#include <tuple>
#include <format>
#include "coap_client_component.h"
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

template<typename... Ts> class CoapClientSendAction : public Action<Ts...> {
 public:
  CoapClientSendAction(CoapClientComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, request_name)
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(std::string, method)
  TEMPLATABLE_VALUE(std::string, media_type)
  TEMPLATABLE_VALUE(std::string, payload)
  TEMPLATABLE_VALUE(size_t, max_response_buffer_size)
  TEMPLATABLE_VALUE(bool, capture_response)
  TEMPLATABLE_VALUE(bool, observe)
  TEMPLATABLE_VALUE(uint32_t, response_timeout)

  void play(Ts... x) override {
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
    this->request_name = this->request_name_.value(x...);
    if (this->request_name.length() == 0) {
      this->request_name = std::format("{}", esphome::micros());
    }
    CoapClientRequestData tx_request = {
        .name = this->request_name,
        .method = this->get_method_(this->method_.value(x...)),
        .uri = this->url_.value(x...),
        .callback = CoapClientSendAction::callback,
        .callback_context = this,
        .media_type = this->get_media_type_(this->media_type_.value(x...)),
        .payload = payload,
        .response_timeout = this->response_timeout_.value(x...),
        .observe = this->observe_.value(x...),
    };

    auto resp_stats = get_response_stats();
    resp_stats->clean();
    this->max_resp_buffer_size = this->max_response_buffer_size_.value(x...);
    this->capture_resp = this->capture_response_.value(x...);
    this->response_payload_ = "";
    this->response_finished = false;
    resp_stats->start_ms = esphome::millis();
    this->captured_args = std::make_tuple(x...);
    this->parent_->process_request(tx_request);
  }

  void trigger_this() {
    auto resp_stats = get_response_stats();
    auto request_name = this->request_name;
    auto captured_args = this->captured_args;

    // response_payload will be "" if capture_response = false;
    std::string response_payload = this->response_payload_;
    std::apply(
        [this, &resp_stats, &request_name, &response_payload](Ts... captured_args_inner) {
          this->success_trigger_->trigger(resp_stats, request_name, response_payload, captured_args_inner...);
        },
        captured_args);
  }

  static void callback(uint16_t response_code, const unsigned char *data, size_t len, size_t offset, size_t total,
                       void *context) {
    CoapClientSendAction<Ts...> *obj = (CoapClientSendAction<Ts...> *) context;
    auto resp_stats = obj->get_response_stats();

    if (len == 0 && offset == 0 && total == 0 && (response_code < 200 || response_code > 299)) {
      // Error
      resp_stats->content_length = 0;
      resp_stats->status_code = response_code;
      resp_stats->duration_ms = esphome::millis() - resp_stats->start_ms;
      resp_stats->start_ms = esphome::millis();
      obj->trigger_this();
      return;
    }
    if (len > 0) {
      if (obj->capture_resp) {
        size_t store_len = std::min(len, obj->max_resp_buffer_size - offset);
        if (store_len > 0) {
          obj->assign_response_payload(data, len, offset);
        }
      }
    }
    if (total > 0 && len + offset == total) {
      resp_stats->content_length = total;
      resp_stats->status_code = response_code;
      resp_stats->duration_ms = esphome::millis() - obj->response_stats->start_ms;
      resp_stats->start_ms = esphome::millis();
      obj->trigger_this();
    }
  }

  void add_json(const char *key, TemplatableValue<std::string, Ts...> value) { this->json_.insert({key, value}); }

  void set_json(std::function<void(Ts..., JsonObject)> json_func) { this->json_func_ = json_func; }

  Trigger<std::shared_ptr<CoapResponseStatistics>, std::string &, std::string &, Ts...> *get_success_trigger() const {
    return this->success_trigger_;
  }
  const std::shared_ptr<CoapResponseStatistics> response_stats = std::make_shared<CoapResponseStatistics>();
  const std::shared_ptr<CoapResponseStatistics> get_response_stats() { return response_stats; }

  bool response_finished{false};

  size_t max_resp_buffer_size{1000};
  std::string request_name{};
  bool capture_resp{false};
  std::tuple<Ts...> captured_args;

  // Blocks, Qblocks don't work with this would need to use a char* buffer and populate with offset
  void assign_response_payload(const unsigned char *data, size_t len, size_t offset) {
    this->response_payload_.assign(reinterpret_cast<const char *>(data), len);
  }

 protected:
  std::string response_payload_;
  CoapMethod get_method_(std::string method) {
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

  CoapMediaType get_media_type_(std::string media_type) {
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

  void encode_json_(Ts... x, JsonObject root) {
    for (const auto &item : this->json_) {
      auto val = item.second;
      root[item.first] = val.value(x...);
    }
  }

  void encode_json_func_(Ts... x, JsonObject root) { this->json_func_(x..., root); }

  CoapClientComponent *parent_;
  std::map<const char *, TemplatableValue<std::string, Ts...>> json_{};
  std::function<void(Ts..., JsonObject)> json_func_{nullptr};

  Trigger<std::shared_ptr<CoapResponseStatistics>, std::string &, std::string &, Ts...> *success_trigger_ =
      new Trigger<std::shared_ptr<CoapResponseStatistics>, std::string &, std::string &, Ts...>();
};

template<typename... Ts> class CoapClientRemoveAction : public Action<Ts...> {
 public:
  CoapClientRemoveAction(CoapClientComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, request_name)

  void play(Ts... x) override { this->parent_->remove(this->request_name_.value(x...)); }

 protected:
  CoapClientComponent *parent_;
};

}  // namespace esphome::coap_client
