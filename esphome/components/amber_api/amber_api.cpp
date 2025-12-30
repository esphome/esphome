#include "amber_api.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace amber_api {

static const char *const TAG = "amber_api";

void AmberApiComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Amber API:");
  ESP_LOGCONFIG(TAG, "  URL: %s", this->url_);
  ESP_LOGCONFIG(TAG, "  Update Interval: %ums", (unsigned) this->get_update_interval());
}

void AmberApiComponent::notify_listeners_() const {
  for (auto *listener : this->listeners_) {
    listener->on_amber_api_update(this->data_);
  }
  for (auto *trigger : this->update_triggers_) {
    trigger->trigger(this->data_);
  }
}

void AmberApiComponent::update() {
  if (this->http_request_ == nullptr) {
    ESP_LOGE(TAG, "HTTP request component not set");
    return;
  }

  // Build the authorization header
  std::list<http_request::Header> headers;
  http_request::Header auth_header{"Authorization", this->auth_header_};
  headers.push_back(auth_header);

  ESP_LOGD(TAG, "Requesting Amber API data from: %s", this->url_);

  // Make the HTTP request
  auto container = this->http_request_->get(this->url_, headers);

  if (container == nullptr) {
    ESP_LOGW(TAG, "Failed to make HTTP request");
    return;
  }

  // Check status code
  if (container->status_code != 200) {
    ESP_LOGW(TAG, "HTTP request failed with status code: %d", container->status_code);
    container->end();
    return;
  }

  // Read the response body
  std::string response_body{};
  if (container->content_length > 0) {
    response_body.reserve(container->content_length);
  }

  uint8_t buffer[512];
  while (container->get_bytes_read() < container->content_length) {
    int bytes_read = container->read(buffer, sizeof(buffer));
    if (bytes_read <= 0) {
      break;
    }
    response_body.append((char *) buffer, bytes_read);
    App.feed_wdt();
    yield();
  }

  container->end();

  ESP_LOGV(TAG, "Response body: %s", response_body.c_str());

  // Parse the JSON response
  this->parse_response_(response_body);
}

void AmberApiComponent::parse_response_(const std::string &response_body) {
  // Parse JSON response
  JsonDocument doc = json::parse_json(response_body);

  if (doc.overflowed()) {
    ESP_LOGE(TAG, "JSON document overflow - response too large");
    return;
  }

  if (doc.isNull()) {
    ESP_LOGE(TAG, "Failed to parse JSON response");
    return;
  }

  if (!doc.is<JsonArray>()) {
    ESP_LOGE(TAG, "Response is not a JSON array");
    return;
  }

  JsonArray array = doc.as<JsonArray>();

  // Process the array to find the current and forecast prices for general and feedIn channels
  for (JsonVariant value : array) {
    JsonObject obj = value.as<JsonObject>();

    if (obj["channelType"].isNull() || obj["type"].isNull()) {
      continue;
    }

    std::string channel_type = obj["channelType"].as<std::string>();
    std::string interval_type = obj["type"].as<std::string>();

    if (channel_type == "general") {
      if (obj["perKwh"].isNull()) {
        continue;
      }
      float per_kwh = obj["perKwh"].as<float>() / 100.0f;

      if (interval_type == "CurrentInterval") {
        this->data_.general_price = per_kwh;

        // Extract spike status and descriptor from general current interval
        if (!obj["spikeStatus"].isNull()) {
          this->data_.spike_status = obj["spikeStatus"].as<std::string>();
        }
        if (!obj["descriptor"].isNull()) {
          this->data_.descriptor = obj["descriptor"].as<std::string>();
        }

        ESP_LOGD(TAG, "General current price: %.2f $/kWh", per_kwh);
      } else if (interval_type == "ForecastInterval") {
        this->data_.general_forecast_price = per_kwh;
        ESP_LOGD(TAG, "General forecast price: %.2f $/kWh", per_kwh);
      }
    } else if (channel_type == "feedIn") {
      if (obj["perKwh"].isNull()) {
        continue;
      }
      float feed_in_per_kwh = obj["perKwh"].as<float>() / -100.0f;

      if (interval_type == "CurrentInterval") {
        this->data_.feedin_price = feed_in_per_kwh;
        ESP_LOGD(TAG, "Feed-in current price: %.2f $/kWh", feed_in_per_kwh);
      } else if (interval_type == "ForecastInterval") {
        this->data_.feedin_forecast_price = feed_in_per_kwh;
        ESP_LOGD(TAG, "Feed-in forecast price: %.2f $/kWh", feed_in_per_kwh);
      }
    }
  }

  // Notify all listeners
  this->notify_listeners_();
}

}  // namespace amber_api
}  // namespace esphome
