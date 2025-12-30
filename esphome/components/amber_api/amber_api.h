#pragma once

#include <functional>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/http_request/http_request.h"
#include "esphome/components/json/json_util.h"

namespace esphome {
namespace amber_api {

struct AmberApiData {
  float general_price{NAN};
  float general_forecast_price{NAN};
  float feedin_price{NAN};
  float feedin_forecast_price{NAN};
  std::string spike_status;
  std::string descriptor;
};

class AmberApiListener {
 public:
  virtual ~AmberApiListener() = default;
  virtual void on_amber_api_update(const AmberApiData &data) = 0;
};

class AmberApiComponent : public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_api_key(const std::string &api_key) { this->api_key_ = api_key; }
  void set_site_id(const std::string &site_id) { this->site_id_ = site_id; }
  void set_http_request(http_request::HttpRequestComponent *http_request) { this->http_request_ = http_request; }

  void register_listener(AmberApiListener *listener) { this->listeners_.push_back(listener); }

  void add_on_update_trigger(Trigger<> *trigger) { this->update_triggers_.push_back(trigger); }
  const AmberApiData &get_data() const { return this->data_; }
  float get_general_price() const { return this->data_.general_price; }
  float get_general_forecast_price() const { return this->data_.general_forecast_price; }
  float get_feedin_price() const { return this->data_.feedin_price; }
  float get_feedin_forecast_price() const { return this->data_.feedin_forecast_price; }
  const std::string &get_spike_status() const { return this->data_.spike_status; }
  const std::string &get_descriptor() const { return this->data_.descriptor; }

 protected:
  void parse_response_(const std::string &response_body);
  void notify_listeners_();

  std::string api_key_;
  std::string site_id_;
  http_request::HttpRequestComponent *http_request_{nullptr};

  std::vector<AmberApiListener *> listeners_;
  std::vector<Trigger<> *> update_triggers_;
  AmberApiData data_;
};

}  // namespace amber_api
}  // namespace esphome
