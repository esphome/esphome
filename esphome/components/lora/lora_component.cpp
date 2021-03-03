#include "lora_component.h"
namespace esphome {
namespace lora {

static const char *TAG = "lora_component";

void LoraComponent::process_lora_packet(LoraPacket lora_packet) {
  switch (lora_packet.data_type) {
    case 0: {
#ifdef USE_SENSOR
      for (auto *lora_sensor : this->sensors_) {
        ESP_LOGD(TAG, "Checking %s %s %s", lora_sensor->sensor->get_object_id().c_str(),
                 lora_packet.sensor_name.c_str(), YESNO(lora_sensor->send_to_lora));

        if (lora_sensor->receive_from_lora && lora_sensor->lora_name == lora_packet.sensor_name) {
          ESP_LOGD(TAG, "Processing %s state %lf", lora_sensor->sensor->get_object_id().c_str(), lora_packet.state);
          lora_sensor->sensor->publish_state(lora_packet.state);
          break;
        }
      }
#endif
      break;
    }
  }
}

#ifdef USE_SENSOR
void LoraComponent::register_sensor(sensor::Sensor *sensor, bool send_to_lora, bool receive_from_lora,
                                    std::string lora_name) {
  LoraSensorComponent *lora_sensor = new LoraSensorComponent();
  lora_sensor->sensor = sensor;
  lora_sensor->receive_from_lora = receive_from_lora;
  lora_sensor->send_to_lora = send_to_lora;

  if (lora_name == "")
    lora_sensor->lora_name = sensor->get_object_id();
  else
    lora_sensor->lora_name = lora_name;

  if (receive_from_lora) {
    this->sensors_.push_back(lora_sensor);
  }

  if (send_to_lora) {
    sensor->add_on_state_callback([this, lora_sensor](float state) { this->process_sensor_(lora_sensor, state); });
  }
}

bool LoraComponent::process_sensor_(LoraSensorComponent *lora_sensor, float state) {
  if (!lora_sensor->send_to_lora)
    return true;

  // App name
  std::string to_send = this->get_app_name_();
  to_send += this->lora_delimiter_;

  // Type
  to_send += "0";
  to_send += this->lora_delimiter_;

  // Sensor Name
  to_send += lora_sensor->sensor->get_object_id();
  to_send += this->lora_delimiter_;

  // State
  to_send += to_string(state);
  to_send += this->lora_delimiter_;

  // // Device Class
  // to_send += lora_sensor->sensor->get_device_class();
  // to_send += this->lora_delimiter_;

  // // ICON
  // to_send += lora_sensor->sensor->get_icon();
  // to_send += this->lora_delimiter_;

  // // UoM
  // to_send += lora_sensor->sensor->get_unit_of_measurement();
  // to_send += this->lora_delimiter_;

  // // Accuracy
  // to_send += to_string(lora_sensor->sensor->get_accuracy_decimals());
  // to_send += this->lora_delimiter_;

  ESP_LOGD(TAG, "Lora Sensor wants to publish %s", to_send.c_str());

  this->send_printf("%s", to_send.c_str());
  return true;
}

#endif

}  // namespace lora
}  // namespace esphome