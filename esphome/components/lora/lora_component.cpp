#include "lora_component.h"
namespace esphome {
namespace lora {

static const char *TAG = "lora_component";

void LoraComponent::process_lora_packet(LoraPacket *lora_packet) {
  LOG_LORA_PACKET(lora_packet);

  this->last_rssi = lora_packet->rssi;
  ESP_LOGD(TAG, "Checking %s component_type %d", lora_packet->component_name.c_str(), lora_packet->component_type);
  switch (lora_packet->component_type) {
    case 0: {
#ifdef USE_SENSOR
      for (auto *lora_component : this->sensors_) {
        if (lora_component->receive_from_lora && lora_component->lora_name == lora_packet->component_name) {
          ESP_LOGD(TAG, "Processing Sensor %s state %lf", lora_component->lora_name.c_str(), lora_packet->state);
          lora_component->component->publish_state(lora_packet->state);
          break;
        }
      }
#endif
      break;
    }
    case 1: {
#ifdef USE_SWITCH
      for (auto *lora_component : this->switches_) {
        if (lora_component->receive_from_lora && lora_component->lora_name == lora_packet->component_name) {
          ESP_LOGD(TAG, "Processing Switch %s state %lf", lora_component->lora_name.c_str(), lora_packet->state);
          lora_component->component->publish_state(lora_packet->state == 0 ? false : true);
          break;
        }
      }
#endif
      break;
    }
    case 2: {
#ifdef USE_BINARY_SENSOR
      for (auto *lora_component : this->binary_sensors_) {
        if (lora_component->receive_from_lora && lora_component->lora_name == lora_packet->component_name) {
          ESP_LOGD(TAG, "Processing Binary Sensor %s state %lf", lora_component->lora_name.c_str(), lora_packet->state);
          lora_component->component->publish_state(lora_packet->state == 0 ? false : true);
          break;
        }
      }
#endif
      break;
    }
    case 3: {
#ifdef USE_TEXT_SENSOR
      for (auto *lora_component : this->text_sensors_) {
        if (lora_component->receive_from_lora && lora_component->lora_name == lora_packet->component_name) {
          ESP_LOGD(TAG, "Processing Text Sensor %s state %s", lora_component->lora_name.c_str(),
                   lora_packet->state_str.c_str());
          lora_component->component->publish_state(lora_packet->state_str);
          break;
        }
      }
#endif
      break;
    }
  }
}

std::string LoraComponent::build_to_send_(std::string type, std::string name, std::string state) {
  // App name
  std::string to_send = this->lora_group_start_delimiter_;
  to_send += this->get_app_name_();
  to_send += this->lora_delimiter_;

  // Type
  to_send += type;
  to_send += this->lora_delimiter_;

  // Sensor Name
  to_send += name;
  to_send += this->lora_delimiter_;

  // State
  to_send += state;
  to_send += this->lora_delimiter_;

  // hash
  auto crc = fnv1_hash(to_send);
  ESP_LOGD(TAG, "CRC %s %d %d %s", to_send.c_str(), to_send.length(), crc, to_string(crc).c_str());
  to_send += to_string(crc);
  to_send += this->lora_group_end_delimiter_;
  // // Device Class
  // to_send += lora_component->sensor->get_device_class();
  // to_send += this->lora_delimiter_;

  // // ICON
  // to_send += lora_component->sensor->get_icon();
  // to_send += this->lora_delimiter_;

  // // UoM
  // to_send += lora_component->sensor->get_unit_of_measurement();
  // to_send += this->lora_delimiter_;

  // // Accuracy
  // to_send += to_string(lora_component->sensor->get_accuracy_decimals());
  // to_send += this->lora_delimiter_;
  return to_send;
}

#if defined(USE_BINARY_SENSOR) || defined(USE_BINARY_SENSOR)
bool LoraComponent::process_component_(LoraBaseComponent *lora_component, float state) {
  if (!lora_component->send_to_lora)
    return true;

  std::string to_send = this->build_to_send_(lora_component->type, lora_component->lora_name, to_string(state));

  ESP_LOGD(TAG, "Sending over lora %s", to_send.c_str());

  this->send_printf("%s", to_send.c_str());
  return true;
}
#endif

#ifdef USE_BINARY_SENSOR
void LoraComponent::register_binary_sensor(binary_sensor::BinarySensor *component, bool send_to_lora,
                                           bool receive_from_lora, std::string lora_name) {
  LoraBinarySensorComponent *lora_component = new LoraBinarySensorComponent();
  lora_component->component = component;
  lora_component->receive_from_lora = receive_from_lora;
  lora_component->send_to_lora = send_to_lora;
  lora_component->type = "2";

  if (lora_name == "")
    lora_component->lora_name = component->get_object_id();
  else
    lora_component->lora_name = lora_name;

  if (receive_from_lora) {
    this->binary_sensors_.push_back(lora_component);
  }

  if (send_to_lora) {
    ESP_LOGD(TAG, "Adding call back for binary sensor %s", lora_component->lora_name.c_str());
    component->add_on_state_callback(
        [this, lora_component](bool state) { this->process_component_(lora_component, state); });
  }
}
#endif
#ifdef USE_TEXT_SENSOR
bool LoraComponent::process_component_(LoraBaseComponent *lora_component, std::string state) {
  if (!lora_component->send_to_lora)
    return true;

  std::string to_send = this->build_to_send_(lora_component->type, lora_component->lora_name, state);

  ESP_LOGD(TAG, "Sending over lora %s", to_send.c_str());

  this->send_printf("%s", to_send.c_str());
  return true;
}

void LoraComponent::register_text_sensor(text_sensor::TextSensor *component, bool send_to_lora, bool receive_from_lora,
                                         std::string lora_name) {
  LoraTextSensorComponent *lora_component = new LoraTextSensorComponent();
  lora_component->component = component;
  lora_component->receive_from_lora = receive_from_lora;
  lora_component->send_to_lora = send_to_lora;
  lora_component->type = "3";

  if (lora_name == "")
    lora_component->lora_name = component->get_object_id();
  else
    lora_component->lora_name = lora_name;

  if (receive_from_lora) {
    this->text_sensors_.push_back(lora_component);
  }

  if (send_to_lora) {
    ESP_LOGD(TAG, "Adding call back for text sensor %s", lora_component->lora_name.c_str());
    component->add_on_state_callback(
        [this, lora_component](std::string state) { this->process_component_(lora_component, state); });
  }
}
#endif
#ifdef USE_SWITCH

bool LoraComponent::process_component_(LoraBaseComponent *lora_component, bool state) {
  if (!lora_component->send_to_lora)
    return true;

  std::string to_send = this->build_to_send_(lora_component->type, lora_component->lora_name, state ? "1" : "0");

  ESP_LOGD(TAG, "Sending over lora %s", to_send.c_str());

  this->send_printf("%s", to_send.c_str());
  return true;
}

void LoraComponent::register_switch(switch_::Switch *component, bool send_to_lora, bool receive_from_lora,
                                    std::string lora_name) {
  LoraSwitchComponent *lora_component = new LoraSwitchComponent();
  lora_component->component = component;
  lora_component->receive_from_lora = receive_from_lora;
  lora_component->send_to_lora = send_to_lora;
  lora_component->type = "1";

  if (lora_name == "")
    lora_component->lora_name = component->get_object_id();
  else
    lora_component->lora_name = lora_name;

  if (receive_from_lora) {
    this->switches_.push_back(lora_component);
  }

  if (send_to_lora) {
    ESP_LOGD(TAG, "Adding call back for switch %s", lora_component->lora_name.c_str());
    component->add_on_state_callback(
        [this, lora_component](bool state) { this->process_component_(lora_component, state); });
  }
}
#endif

#ifdef USE_SENSOR
void LoraComponent::register_sensor(sensor::Sensor *sensor, bool send_to_lora, bool receive_from_lora,
                                    std::string lora_name) {
  LoraSensorComponent *lora_component = new LoraSensorComponent();
  lora_component->component = sensor;
  lora_component->receive_from_lora = receive_from_lora;
  lora_component->send_to_lora = send_to_lora;
  lora_component->type = "0";

  if (lora_name == "")
    lora_component->lora_name = sensor->get_object_id();
  else
    lora_component->lora_name = lora_name;

  if (receive_from_lora) {
    this->sensors_.push_back(lora_component);
  }

  if (send_to_lora) {
    ESP_LOGD(TAG, "Adding call back for sensor %s", lora_component->lora_name.c_str());
    sensor->add_on_state_callback(
        [this, lora_component](float state) { this->process_component_(lora_component, state); });
  }
}
#endif

}  // namespace lora
}  // namespace esphome
