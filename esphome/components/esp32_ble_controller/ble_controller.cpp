#include "ble_controller.h"

#include "esphome/core/application.h"
#include "esphome/components/esp32_ble_server/ble_2901.h"
#include "esphome/components/esp32_ble_server/ble_2902.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_ble_controller {

static const char *const ESPHOME_SERVICE_UUID = "03774663-d394-496e-8dcd-000000000000";
static const char *const LOGGER_CHARACTERISTIC_UUID = "03774663-d394-496e-8dcd-000000000001";

static const char *const BINARY_SENSOR_SERVICE_UUID = "03774663-d394-496e-8dcd-000100000000";
static const char *const COVER_SERVICE_UUID = "03774663-d394-496e-8dcd-000200000000";
static const char *const FAN_SERVICE_UUID = "03774663-d394-496e-8dcd-000300000000";
static const char *const LIGHT_SERVICE_UUID = "03774663-d394-496e-8dcd-000400000000";
static const char *const SENSOR_SERVICE_UUID = "03774663-d394-496e-8dcd-000500000000";
static const char *const SWITCH_SERVICE_UUID = "03774663-d394-496e-8dcd-000600000000";
static const char *const TEXT_SENSOR_SERVICE_UUID = "03774663-d394-496e-8dcd-000700000000";
static const char *const CLIMATE_SERVICE_UUID = "03774663-d394-496e-8dcd-000800000000";

static const char *const TAG = "esp32_ble_controller";

void BLEController::setup() {
  ESP_LOGD(TAG, "Setting up BLE controller");
  this->esphome_service_ = global_ble_server->create_service(ESPHOME_SERVICE_UUID);

#ifdef USE_LOGGER
  {
    this->logger_characteristic_ = this->esphome_service_->create_characteristic(
        LOGGER_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

    BLEDescriptor *logger_name = new BLE2901("Logger");
    this->logger_characteristic_->add_descriptor(logger_name);

    BLEDescriptor *descriptor_2902 = new BLE2902();
    this->logger_characteristic_->add_descriptor(descriptor_2902);
  }
#endif

#ifdef USE_BINARY_SENSOR
  {
    auto binary_sensors = App.get_binary_sensors();
    if (!binary_sensors.empty()) {
      this->binary_sensor_service_ = global_ble_server->create_service(BINARY_SENSOR_SERVICE_UUID);
      for (auto *obj : binary_sensors) {
        std::string uuid = std::string(BINARY_SENSOR_SERVICE_UUID).substr(0, 28);
        uuid += uint32_to_string(obj->get_object_id_hash());
        BLECharacteristic *characteristic = this->binary_sensor_service_->create_characteristic(
            uuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

        BLEDescriptor *name = new BLE2901(obj->get_name());
        characteristic->add_descriptor(name);

        BLEDescriptor *descriptor = new BLE2902();
        characteristic->add_descriptor(descriptor);

        this->characteristics_.insert(
            std::pair<uint32_t, BLECharacteristic *>(obj->get_object_id_hash(), characteristic));
      }
    }
  }
#endif
#ifdef USE_COVER
  if (!App.get_covers().empty()) {
    this->cover_service_ = global_ble_server->create_service(COVER_SERVICE_UUID);
  }
#endif
#ifdef USE_FAN
  if (!App.get_fans().empty()) {
    this->fan_service_ = global_ble_server->create_service(FAN_SERVICE_UUID);
  }
#endif
#ifdef USE_LIGHT
  if (!App.get_lights().empty()) {
    this->light_service_ = global_ble_server->create_service(LIGHT_SERVICE_UUID);
  }
#endif
#ifdef USE_SENSOR
  {
    auto sensors = App.get_sensors();
    if (!sensors.empty()) {
      this->sensor_service_ = global_ble_server->create_service(SENSOR_SERVICE_UUID);
      for (auto *obj : sensors) {
        std::string uuid = std::string(SENSOR_SERVICE_UUID).substr(0, 28);
        uuid += uint32_to_string(obj->get_object_id_hash());
        BLECharacteristic *characteristic = this->sensor_service_->create_characteristic(
            uuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

        BLEDescriptor *name = new BLE2901(obj->get_name());
        characteristic->add_descriptor(name);

        BLEDescriptor *descriptor = new BLE2902();
        characteristic->add_descriptor(descriptor);

        this->characteristics_.insert(
            std::pair<uint32_t, BLECharacteristic *>(obj->get_object_id_hash(), characteristic));
      }
    }
  }
#endif
#ifdef USE_SWITCH
  {
    auto switches = App.get_switches();
    if (!switches.empty()) {
      this->switch_service_ = global_ble_server->create_service(SWITCH_SERVICE_UUID);
      for (auto *obj : switches) {
        std::string uuid = std::string(SWITCH_SERVICE_UUID).substr(0, 28);
        uuid += uint32_to_string(obj->get_object_id_hash());
        BLECharacteristic *characteristic = this->switch_service_->create_characteristic(
            uuid,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE);

        BLEDescriptor *name = new BLE2901(obj->get_name());
        characteristic->add_descriptor(name);

        BLEDescriptor *descriptor = new BLE2902();
        characteristic->add_descriptor(descriptor);

        this->characteristics_.insert(
            std::pair<uint32_t, BLECharacteristic *>(obj->get_object_id_hash(), characteristic));

        characteristic->on_write([obj](std::vector<uint8_t> data) {
          if (data[0])
            obj->turn_on();
          else
            obj->turn_off();
        });
      }
    }
  }
#endif
#ifdef USE_TEXT_SENSOR
  {
    auto text_sensors = App.get_text_sensors();
    if (!text_sensors.empty()) {
      this->text_sensor_service_ = global_ble_server->create_service(TEXT_SENSOR_SERVICE_UUID);
      for (auto *obj : text_sensors) {
        std::string uuid = std::string(TEXT_SENSOR_SERVICE_UUID).substr(0, 28);
        uuid += uint32_to_string(obj->get_object_id_hash());
        BLECharacteristic *characteristic = this->text_sensor_service_->create_characteristic(
            uuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

        BLEDescriptor *name = new BLE2901(obj->get_name());
        characteristic->add_descriptor(name);

        BLEDescriptor *descriptor = new BLE2902();
        characteristic->add_descriptor(descriptor);

        this->characteristics_.insert(
            std::pair<uint32_t, BLECharacteristic *>(obj->get_object_id_hash(), characteristic));
      }
    }
  }
#endif
#ifdef USE_CLIMATE
  if (!App.get_climates().empty()) {
    this->cover_service_ = global_ble_server->create_service(COVER_SERVICE_UUID);
  }
#endif
  this->state_ = CREATING;
  this->setup_controller();
}

void BLEController::loop() {
  switch (this->state_) {
    case CREATING: {
      bool all_created = true;
      all_created &= this->esphome_service_->is_created();
#ifdef USE_BINARY_SENSOR
      all_created &= this->binary_sensor_service_ == nullptr || this->binary_sensor_service_->is_created();
#endif
#ifdef USE_COVER
      all_created &= this->cover_service_ == nullptr || this->cover_service_->is_created();
#endif
#ifdef USE_FAN
      all_created &= this->fan_service_ == nullptr || this->fan_service_->is_created();
#endif
#ifdef USE_LIGHT
      all_created &= this->light_service_ == nullptr || this->light_service_->is_created();
#endif
#ifdef USE_SENSOR
      all_created &= this->sensor_service_ == nullptr || this->sensor_service_->is_created();
#endif
#ifdef USE_SWITCH
      all_created &= this->switch_service_ == nullptr || this->switch_service_->is_created();
#endif
#ifdef USE_TEXT_SENSOR
      all_created &= this->text_sensor_service_ == nullptr || this->text_sensor_service_->is_created();
#endif
#ifdef USE_CLIMATE
      all_created &= this->climate_service_ == nullptr || this->climate_service_->is_created();
#endif
      if (all_created) {
        ESP_LOGI(TAG, "All services created");
        this->state_ = STARTING;
      }
      break;
    }
    case STARTING: {
      bool all_running = true;

      all_running &= this->esphome_service_->is_running();

#ifdef USE_BINARY_SENSOR
      all_running &= this->binary_sensor_service_ == nullptr || this->binary_sensor_service_->is_running();
#endif
#ifdef USE_COVER
      all_running &= this->cover_service_ == nullptr || this->cover_service_->is_running();
#endif
#ifdef USE_FAN
      all_running &= this->fan_service_ == nullptr || this->fan_service_->is_running();
#endif
#ifdef USE_LIGHT
      all_running &= this->light_service_ == nullptr || this->light_service_->is_running();
#endif
#ifdef USE_SENSOR
      all_running &= this->sensor_service_ == nullptr || this->sensor_service_->is_running();
#endif
#ifdef USE_SWITCH
      all_running &= this->switch_service_ == nullptr || this->switch_service_->is_running();
#endif
#ifdef USE_TEXT_SENSOR
      all_running &= this->text_sensor_service_ == nullptr || this->text_sensor_service_->is_running();
#endif
#ifdef USE_CLIMATE
      all_running &= this->climate_service_ == nullptr || this->climate_service_->is_running();
#endif
      if (all_running) {
        ESP_LOGD(TAG, "BLE Controller started");
        this->state_ = RUNNING;
#ifdef USE_LOGGER
        logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message) {
          if (level > this->log_level_)
            return;
          std::string log;
          log += "[";
          log += tag;
          log += "] ";
          log += message;
          this->logger_characteristic_->set_value(log);
          this->logger_characteristic_->notify();
        });
#endif
      } else {
        this->esphome_service_->start();
#ifdef USE_BINARY_SENSOR
        this->binary_sensor_service_->start();
#endif
#ifdef USE_COVER
        this->cover_service_->start();
#endif
#ifdef USE_FAN
        this->fan_service_->start();
#endif
#ifdef USE_LIGHT
        this->light_service_->start();
#endif
#ifdef USE_SENSOR
        this->sensor_service_->start();
#endif
#ifdef USE_SWITCH
        this->switch_service_->start();
#endif
#ifdef USE_TEXT_SENSOR
        this->text_sensor_service_->start();
#endif
#ifdef USE_CLIMATE
        this->climate_service_->start();
#endif
      }
      break;
    }

    case RUNNING:
    case INIT:
      break;
    default:
      break;
  }
}

void BLEController::start() {
  if (this->state_ == RUNNING)
    return;
  this->state_ = STARTING;
}
void BLEController::stop() {
  this->esphome_service_->stop();
#ifdef USE_BINARY_SENSOR
  this->binary_sensor_service_->stop();
#endif
#ifdef USE_COVER
  this->cover_service_->stop();
#endif
#ifdef USE_FAN
  this->fan_service_->stop();
#endif
#ifdef USE_LIGHT
  this->light_service_->stop();
#endif
#ifdef USE_SENSOR
  this->sensor_service_->stop();
#endif
#ifdef USE_SWITCH
  this->switch_service_->stop();
#endif
#ifdef USE_TEXT_SENSOR
  this->text_sensor_service_->stop();
#endif
#ifdef USE_CLIMATE
  this->climate_service_->stop();
#endif
}
float BLEController::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

#ifdef USE_BINARY_SENSOR
void BLEController::on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) {
  if (obj->is_internal())
    return;
  auto *characteristic = this->characteristics_[obj->get_object_id_hash()];
  characteristic->set_value(state);
  characteristic->notify();
}
#endif
#ifdef USE_COVER
void BLEController::on_cover_update(cover::Cover *obj) {
  if (obj->is_internal())
    return;
}
#endif
#ifdef USE_FAN
void BLEController::on_fan_update(fan::FanState *obj) {
  if (obj->is_internal())
    return;
}
#endif
#ifdef USE_LIGHT
void BLEController::on_light_update(light::LightState *obj) {
  if (obj->is_internal())
    return;
}
#endif
#ifdef USE_SENSOR
void BLEController::on_sensor_update(sensor::Sensor *obj, float state) {
  if (obj->is_internal())
    return;
  auto *characteristic = this->characteristics_[obj->get_object_id_hash()];
  characteristic->set_value(state);
  characteristic->notify();
}
#endif
#ifdef USE_SWITCH
void BLEController::on_switch_update(switch_::Switch *obj, bool state) {
  if (obj->is_internal())
    return;
  auto *characteristic = this->characteristics_[obj->get_object_id_hash()];
  characteristic->set_value(state);
  characteristic->notify();
}
#endif
#ifdef USE_TEXT_SENSOR
void BLEController::on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state) {
  if (obj->is_internal())
    return;
  auto *characteristic = this->characteristics_[obj->get_object_id_hash()];
  characteristic->set_value(state);
  characteristic->notify();
}
#endif
#ifdef USE_CLIMATE
void BLEController::on_climate_update(climate::Climate *obj) {
  if (obj->is_internal())
    return;
}
#endif

}  // namespace esp32_ble_controller
}  // namespace esphome

#endif
