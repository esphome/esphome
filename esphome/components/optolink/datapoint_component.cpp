#ifdef USE_ARDUINO

#include "datapoint_component.h"
#include "optolink.h"
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.datapoint_component";
static std::vector<HassSubscription> hass_subscriptions_;

void DatapointComponent::setup_datapoint_() {
  switch (div_ratio_) {
    case 0:
      datapoint_ = new Datapoint<convRaw>(get_component_name().c_str(), "optolink", address_, writeable_);
      datapoint_->setLength(bytes_);
      datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
        uint8_t buffer[bytes_];
        dp_value.getRaw(buffer);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_INFO
        char print_buffer[bytes_ * 2 + 1];
        dp_value.getString(print_buffer, sizeof(print_buffer));
        ESP_LOGI(TAG, "recieved data for datapoint %s: %s", dp.getName(), print_buffer);
#endif
        datapoint_value_changed((uint8_t *) buffer, bytes_);
        read_retries_ = 0;
      });
      break;
    case 1:
      switch (bytes_) {
        case 1:
          datapoint_ = new Datapoint<conv1_1_US>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %d", dp.getName(), dp_value.getU8());
            datapoint_value_changed(dp_value.getU8());
            read_retries_ = 0;
          });
          break;
        case 2:
          datapoint_ = new Datapoint<conv2_1_US>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %d", dp.getName(), dp_value.getU16());
            datapoint_value_changed(dp_value.getU16());
            read_retries_ = 0;
          });
          break;
        case 4:
          datapoint_ = new Datapoint<conv4_1_UL>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %d", dp.getName(), dp_value.getU32());
            datapoint_value_changed((uint32_t) dp_value.getU32());
            read_retries_ = 0;
          });
          break;
        default:
          unfitting_value_type_();
      }
      break;
    case 10:
      switch (bytes_) {
        case 1:
          datapoint_ = new Datapoint<conv1_10_F>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %f", dp.getName(), dp_value.getFloat());
            datapoint_value_changed(dp_value.getFloat());
            read_retries_ = 0;
          });
          break;
        case 2:
          datapoint_ = new Datapoint<conv2_10_F>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %f", dp.getName(), dp_value.getFloat());
            datapoint_value_changed(dp_value.getFloat());
            read_retries_ = 0;
          });
          break;
        default:
          unfitting_value_type_();
      }
      break;
    case 100:
      switch (bytes_) {
        case 2:
          datapoint_ = new Datapoint<conv2_100_F>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %f", dp.getName(), dp_value.getFloat());
            datapoint_value_changed(dp_value.getFloat());
            read_retries_ = 0;
          });
          break;
        default:
          unfitting_value_type_();
      }
      break;
    case 1000:
      switch (bytes_) {
        case 4:
          datapoint_ = new Datapoint<conv4_1000_F>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %f", dp.getName(), dp_value.getFloat());
            datapoint_value_changed(dp_value.getFloat());
            read_retries_ = 0;
          });
          break;
      }
      break;
    case 3600:
      switch (bytes_) {
        case 4:
          datapoint_ = new Datapoint<conv4_3600_F>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %f", dp.getName(), dp_value.getFloat());
            datapoint_value_changed(dp_value.getFloat());
            read_retries_ = 0;
          });
          break;
      }
      break;
    default:
      unfitting_value_type_();
  }
}

void DatapointComponent::datapoint_read_request_() {
  if (is_dp_value_writing_outstanding_) {
    ESP_LOGI(TAG, "read request for %s deferred due to outstanding write request", get_component_name().c_str());
    datapoint_write_request_(dp_value_outstanding_);
  } else {
    if (read_retries_ == 0 || read_retries_ >= max_retries_until_reset_) {
      if (optolink_->read_value(datapoint_)) {
        read_retries_ = 1;
      }
    } else {
      read_retries_++;
      ESP_LOGW(TAG, "%d. read request for %s rejected  due to outstanding running request - increase update_interval!",
               read_retries_, get_component_name().c_str());
    }
  }
}

void DatapointComponent::datapoint_value_changed(float value) {
  ESP_LOGW(TAG, "unused value update by sensor %s", get_component_name().c_str());
}

void DatapointComponent::datapoint_value_changed(uint8_t value) {
  ESP_LOGW(TAG, "unused value update by sensor %s", get_component_name().c_str());
}

void DatapointComponent::datapoint_value_changed(uint16_t value) {
  ESP_LOGW(TAG, "unused value update by sensor %s", get_component_name().c_str());
}

void DatapointComponent::datapoint_value_changed(uint32_t value) {
  ESP_LOGW(TAG, "unused value update by sensor %s", get_component_name().c_str());
}

void DatapointComponent::datapoint_value_changed(std::string value) {
  ESP_LOGW(TAG, "unused value update by sensor %s", get_component_name().c_str());
}

void DatapointComponent::datapoint_value_changed(uint8_t *value, size_t length) {
  ESP_LOGW(TAG, "unused value update by sensor %s", get_component_name().c_str());
}

void DatapointComponent::datapoint_write_request_(DPValue dp_value) {
  if (!writeable_) {
    optolink_->set_state("trying to control not writable datapoint %s", get_component_name().c_str());
    ESP_LOGE(TAG, "trying to control not writable datapoint %s", get_component_name().c_str());
  } else if (datapoint_ != nullptr) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_INFO
    char buffer[100];
    dp_value.getString(buffer, sizeof(buffer));
    ESP_LOGI(TAG, "trying to update datapoint %s value: %s", get_component_name().c_str(), buffer);
#endif

    dp_value_outstanding_ = dp_value;
    if (optolink_->write_value(datapoint_, dp_value_outstanding_)) {
      is_dp_value_writing_outstanding_ = false;
    } else {
      ESP_LOGW(TAG, "write request for %s rejected due to outstanding running request - increase update_interval!",
               get_component_name().c_str());
      is_dp_value_writing_outstanding_ = true;
    }
  }
}

void DatapointComponent::write_datapoint_value_(float value) {
  if (div_ratio_ > 1) {
    datapoint_write_request_(DPValue(value));
  } else if (div_ratio_ == 1) {
    switch (bytes_) {
      case 1:
        datapoint_write_request_(DPValue((uint8_t) value));
        break;
      case 2:
        datapoint_write_request_(DPValue((uint16_t) value));
        break;
      case 4:
        datapoint_write_request_(DPValue((uint32_t) value));
        break;
      default:
        unfitting_value_type_();
        break;
    }
  } else {
    unfitting_value_type_();
  }
}

void DatapointComponent::write_datapoint_value_(uint8_t value) {
  if (bytes_ == 1 && div_ratio_ == 1) {
    datapoint_write_request_(DPValue(value));
  } else {
    unfitting_value_type_();
  }
}

void DatapointComponent::write_datapoint_value_(uint16_t value) {
  if (bytes_ == 2 && div_ratio_ == 1) {
    datapoint_write_request_(DPValue(value));
  } else {
    unfitting_value_type_();
  }
}

void DatapointComponent::write_datapoint_value_(uint32_t value) {
  if (bytes_ == 4 && div_ratio_ == 1) {
    datapoint_write_request_(DPValue(value));
  } else {
    unfitting_value_type_();
  }
}

void DatapointComponent::write_datapoint_value_(uint8_t *value, size_t length) {
  if (bytes_ == length && div_ratio_ == 0) {
    datapoint_write_request_(DPValue(value, length));
  } else {
    unfitting_value_type_();
  }
}

void DatapointComponent::unfitting_value_type_() {
  optolink_->set_state("Unfitting byte/div_ratio combination for sensor/component %s", get_component_name().c_str());
  ESP_LOGE(TAG, "Unfitting byte/div_ratio combination for sensor/component %s", get_component_name().c_str());
}

void DatapointComponent::set_optolink_state_(const char *format, ...) {
  va_list args;
  va_start(args, format);
  char buffer[128];
  std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  optolink_->set_state(buffer);
}

std::string DatapointComponent::get_optolink_state_() { return optolink_->get_state(); }

void DatapointComponent::subscribe_hass_(const std::string entity_id, std::function<void(std::string)> f) {
  for (auto &subscription : hass_subscriptions_) {
    if (subscription.entity_id == entity_id) {
      subscription.callbacks.push_back(f);
      return;
    }
  }
  HassSubscription subscription{entity_id};
  subscription.callbacks.push_back(f);
  hass_subscriptions_.push_back(subscription);

#ifdef USE_API
  if (api::global_api_server != nullptr) {
    api::global_api_server->subscribe_home_assistant_state(
        entity_id, optional<std::string>(), [entity_id](const std::string &state) {
          ESP_LOGD(TAG, "received schedule plan from HASS entity '%s': %s", entity_id.c_str(), state.c_str());
          for (auto &subscription : hass_subscriptions_) {
            if (subscription.last_state != state) {
              if (subscription.entity_id == entity_id) {
                subscription.last_state = state;
                for (const auto &callback : subscription.callbacks) {
                  callback(state);
                }
              }
            }
          }
        });
  }
#endif
}

void conv2_100_F::encode(uint8_t *out, DPValue in) {
  int16_t tmp = floor((in.getFloat() * 100) + 0.5);
  out[1] = tmp >> 8;
  out[0] = tmp & 0xFF;
}

DPValue conv2_100_F::decode(const uint8_t *in) {
  int16_t tmp = in[1] << 8 | in[0];
  DPValue out(tmp / 100.0f);
  return out;
}

void conv4_1000_F::encode(uint8_t *out, DPValue in) {
  int32_t tmp = floor((in.getFloat() * 1000) + 0.5);
  out[3] = tmp >> 24;
  out[2] = tmp >> 16;
  out[1] = tmp >> 8;
  out[0] = tmp & 0xFF;
}

DPValue conv4_1000_F::decode(const uint8_t *in) {
  int32_t tmp = in[3] << 24 | in[2] << 16 | in[1] << 8 | in[0];
  DPValue out(tmp / 1000.0f);
  return out;
}

}  // namespace optolink
}  // namespace esphome

#endif
