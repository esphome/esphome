#include "miio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/components/network/util.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace miio {

static const char *const TAG = "miio";

std::string stringify_prop(prop_t property) {
  return str_sprintf("%u %u", std::get<0>(property), std::get<1>(property));
}

void Miio::setup() {
  while (this->available()) {
    this->read();
  }

  this->mcu_handlers_["mcu_version"] = [this](const std::vector<std::string> &tokens) {
    ESP_LOGI(TAG, "Connected to MIIO MCU, version is %s", tokens.front().c_str());

    this->mcu_reply_ok_();
  };

  this->mcu_handlers_["model"] = [this](const std::vector<std::string> &tokens) {
    ESP_LOGI(TAG, "Product is %s", tokens.front().c_str());

    this->mcu_reply_ok_();
  };

  this->mcu_handlers_["net"] = [this](const std::vector<std::string> &tokens) {
    this->mcu_reply_(this->get_network_state_());
  };

  this->mcu_handlers_["time"] = [this](const std::vector<std::string> &tokens) {
    if (this->time_id_.has_value()) {
      auto time = this->time_id_.value()->now();
      auto serialized = time.strftime("%Y-%m-%d %H:%M:%S");

      this->mcu_reply_(serialized.c_str());
    }
  };

  this->mcu_handlers_["country_code"] = [this](const std::vector<std::string> &tokens) { this->mcu_reply_("EU"); };

  this->mcu_handlers_["ble_config"] = [this](const std::vector<std::string> &tokens) { this->mcu_reply_ok_(); };

  this->mcu_handlers_["get_down"] = [this](const std::vector<std::string> &tokens) { this->process_down_request_(); };

  this->mcu_handlers_["event_occured"] = [this](const std::vector<std::string> &tokens) { this->mcu_reply_ok_(); };

  this->mcu_handlers_["result"] = [this](const std::vector<std::string> &tokens) {
    this->mcu_reply_ok_();
    this->process_result_(tokens);
  };

  this->mcu_handlers_["properties_changed"] = [this](const std::vector<std::string> &tokens) {
    this->mcu_reply_ok_();
    this->process_properties_change_(tokens);
  };
}

void Miio::loop() {
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    this->handle_char_(c);
  }

  this->process_command_queue_();
}

void Miio::dump_config() {
  ESP_LOGCONFIG(TAG, "MIIO:");
  this->check_uart_settings(115200);
}

void Miio::register_listener(prop_t property, MiioPropertyType type,
                             const std::function<void(const MiioPropertyValue &)> &fn) {
  this->mcu_query_.insert(property);
  this->listeners_[property] = std::make_tuple(type, fn);
}

void Miio::set_property(prop_t property, std::string value) { this->mcu_pending_changes_[property] = std::move(value); }

void Miio::process_command_queue_() {
  if (!this->mcu_message_queue_.empty()) {
    const auto &tokens = std::as_const(this->mcu_message_queue_.front());

    this->process_command_raw_(tokens);

    this->mcu_message_queue_.pop_front();
  }
}

void Miio::process_command_raw_(const std::vector<std::string> &tokens) {
  const auto &command = tokens.front();
  auto it = this->mcu_handlers_.find(command);

  if (it != this->mcu_handlers_.end()) {
    std::vector<std::string> v(tokens.begin() + 1, tokens.end());

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    std::string accumulator = "RX";

    for (const auto &token : tokens) {
      accumulator += " ";
      accumulator += token;
    }

    ESP_LOGVV(TAG, "%s", accumulator.c_str());
#endif

    it->second(v);
  } else {
    ESP_LOGW(TAG, "Received unknown command from MCU: %s", command.c_str());
  }
}

void Miio::process_properties_change_(const std::vector<std::string> &tokens) {
  if (tokens.size() % 3 != 0) {
    ESP_LOGE(TAG, "Malformed properties_changed arguments: mod by 3 != 0");
    return;
  }

  for (size_t i = 0; i < tokens.size(); i += 3) {
    auto maybe_prop = this->parse_property_specifier_(tokens[i], tokens[i + 1]);

    if (maybe_prop.has_value()) {
      this->apply_property_(maybe_prop.value(), tokens[i + 2]);
    }
  }
}

void Miio::process_result_(const std::vector<std::string> &tokens) {
  if (!this->awaiting_for_get_properties_result_) {
    return;
  }

  if (tokens.size() % 4 != 0) {
    ESP_LOGE(TAG, "Malformed result arguments: mod by 4 != 0");
    return;
  }

  this->awaiting_for_get_properties_result_ = false;

  for (size_t i = 0; i < tokens.size(); i += 4) {
    auto maybe_prop = this->parse_property_specifier_(tokens[i], tokens[i + 1]);

    if (!maybe_prop.has_value()) {
      continue;
    }

    auto prop = maybe_prop.value();
    auto maybe_result = parse_number<uint8_t>(tokens[i + 2]);

    if (!maybe_result.has_value() || maybe_result.value() != 0) {
      ESP_LOGW(TAG, "Non-zero result code of property %s: %s", stringify_prop(prop).c_str(), tokens[i + 2].c_str());
      continue;
    }

    this->apply_property_(prop, tokens[i + 3]);
  }
}

void Miio::process_down_request_() {
  if (!this->mcu_pending_changes_.empty()) {
    std::string query = "down set_properties";

    for (const auto &change : this->mcu_pending_changes_) {
      query += " " + stringify_prop(change.first);
      query += " " + change.second;
    }

    this->mcu_reply_(query);
    this->mcu_pending_changes_.clear();

    return;
  }

  if (!this->mcu_query_.empty()) {
    std::string query = "down get_properties";

    for (const auto &prop : this->mcu_query_) {
      query += " " + stringify_prop(prop);
    }

    this->mcu_reply_(query);
    this->mcu_query_.clear();
    this->awaiting_for_get_properties_result_ = true;

    return;
  }

  this->mcu_reply_("down none");
}

void Miio::apply_property_(prop_t prop, const std::string &value) {
  auto it = this->listeners_.find(prop);

  if (it == this->listeners_.end()) {
    ESP_LOGV(TAG, "Handler was not found for %s", stringify_prop(prop).c_str());
    return;
  }

  auto excepted_type = std::get<0>(it->second);
  auto callback = std::get<1>(it->second);

  auto maybe_value = this->parse_property_value_(excepted_type, value);

  if (maybe_value.has_value()) {
    callback(maybe_value.value());
  } else {
    ESP_LOGW(TAG, "Failed to parse property %s value: %s", stringify_prop(prop).c_str(), value.c_str());
  }
}

optional<prop_t> Miio::parse_property_specifier_(const std::string &cluster, const std::string &key) {
  auto maybe_cluster = parse_number<uint8_t>(cluster);
  auto maybe_key = parse_number<uint8_t>(key);

  if (maybe_cluster.has_value() && maybe_key.has_value()) {
    return std::make_tuple(maybe_cluster.value(), maybe_key.value());
  }

  return {};
}

optional<MiioPropertyValue> Miio::parse_property_value_(MiioPropertyType type, const std::string &value) {
  MiioPropertyValue p = {.type = type, .value = value, .value_bool = false};

  switch (type) {
    case MiioPropertyType::BOOLEAN: {
      p.value_bool = value == "true";
      return p;
    };

    case MiioPropertyType::NUMBER: {
      auto option = parse_number<float>(value);

      if (option.has_value()) {
        p.value_numeric = option.value();
        return p;
      }

      break;
    };

    case MiioPropertyType::ENUM: {
      auto option = parse_number<uint8_t>(value);

      if (option.has_value()) {
        p.value_enum = option.value();
        return p;
      }

      break;
    };
  }

  return {};
}

const char *Miio::get_network_state_() {
  if (!network::is_connected()) {
    return "offline";
  }

  return remote_is_connected() ? "cloud" : "local";
}

void Miio::mcu_reply_(const std::string &message) { this->mcu_reply_(message.c_str()); }

void Miio::mcu_reply_(const char *message) {
  ESP_LOGVV(TAG, "TX %s", message);

  this->write_str(message);
  this->write('\r');
}

void Miio::mcu_reply_ok_() { this->mcu_reply_("ok"); }

void Miio::handle_char_(uint8_t c) {
  if (this->rx_message_.empty()) {
    this->rx_message_.emplace_back();
  }

  if (c == ' ') {
    this->rx_message_.emplace_back();
  } else if (c == '\r') {
    this->mcu_message_queue_.push_back(std::move(this->rx_message_));
  } else {
    this->rx_message_.back() += c;
  }
}

}  // namespace miio
}  // namespace esphome
