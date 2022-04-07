#pragma once

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/components/esp32_ble_server/ble_2901.h"
#include "esphome/components/esp32_ble_server/ble_2902.h"
#include "esphome/components/esp32_ble_server/ble_characteristic.h"
#include "esphome/components/esp32_ble_server/ble_server.h"
#include <map>

#ifdef USE_LOGGER
#include "esphome/core/log.h"
#endif

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_controller {

using namespace esp32_ble_server;

class BLEController : public Component, public Controller, public esp32_ble_server::BLEServiceComponent {
 public:
  void loop() override;
  void setup() override;
  void start() override;
  void stop() override;

  float get_setup_priority() const override;

#ifdef USE_LOGGER
  void set_log_level(int level) { this->log_level_ = level; }
#endif

#ifdef USE_BINARY_SENSOR
  void on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) override;
#endif
#ifdef USE_COVER
  void on_cover_update(cover::Cover *obj) override;
#endif
#ifdef USE_FAN
  void on_fan_update(fan::FanState *obj) override;
#endif
#ifdef USE_LIGHT
  void on_light_update(light::LightState *obj) override;
#endif
#ifdef USE_SENSOR
  void on_sensor_update(sensor::Sensor *obj, float state) override;
#endif
#ifdef USE_SWITCH
  void on_switch_update(switch_::Switch *obj, bool state) override;
#endif
#ifdef USE_TEXT_SENSOR
  void on_text_sensor_update(text_sensor::TextSensor *obj, const std::string &state) override;
#endif
#ifdef USE_CLIMATE
  void on_climate_update(climate::Climate *obj) override;
#endif

 protected:
  enum State : uint8_t {
    FAILED = 0x00,
    INIT,
    CREATING,
    STARTING,
    RUNNING,
  } state_{INIT};

  std::map<uint32_t, BLECharacteristic *> characteristics_;

  //BLEService *esphome_service_;
  std::shared_ptr<BLEService> esphome_service_;
#ifdef USE_LOGGER
  BLECharacteristic *logger_characteristic_;
  int log_level_{ESPHOME_LOG_LEVEL_DEBUG};
#endif

#ifdef USE_BINARY_SENSOR
  std::shared_ptr<BLEService> binary_sensor_service_;
#endif
#ifdef USE_COVER
  std::shared_ptr<BLEService> cover_service_;
#endif
#ifdef USE_FAN
  std::shared_ptr<BLEService> fan_service_;
#endif
#ifdef USE_LIGHT
  std::shared_ptr<BLEService> light_service_;
#endif
#ifdef USE_SENSOR
  std::shared_ptr<BLEService> sensor_service_;
#endif
#ifdef USE_SWITCH
  std::shared_ptr<BLEService> switch_service_;
#endif
#ifdef USE_TEXT_SENSOR
  std::shared_ptr<BLEService> text_sensor_service_;
#endif
#ifdef USE_CLIMATE
  std::shared_ptr<BLEService> climate_service_;
#endif
  /// Convert a uint32_t to a hex string
  std::string uint32_to_string(uint32_t num);
};

}  // namespace esp32_ble_controller
}  // namespace esphome

#endif
