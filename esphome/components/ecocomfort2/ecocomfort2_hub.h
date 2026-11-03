#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "ecocomfort2_child.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

namespace esphome::ecocomfort2 {

namespace espbt = esphome::esp32_ble_tracker;

// C_OPER byte 0: operating mode
static constexpr uint8_t OPER_OFF = 0x00;
static constexpr uint8_t OPER_IN = 0x01;
static constexpr uint8_t OPER_OUT = 0x02;
static constexpr uint8_t OPER_IN_OUT = 0x03;
static constexpr uint8_t OPER_SENSOR_OR_AUTO = 0x04;

// Speed byte: speed in the low nibble, flags in the high nibble.
// C_OPER carries FLAG_AUTO; the unit reports FLAG_BOOST in byte 1 of C_STATE.
static constexpr uint8_t SPEED_OFF = 0x00;
static constexpr uint8_t SPEED_SLEEP = 0x01;
static constexpr uint8_t SPEED_VEL1 = 0x02;
static constexpr uint8_t SPEED_VEL2 = 0x03;
static constexpr uint8_t SPEED_VEL3 = 0x04;
static constexpr uint8_t SPEED_MASK = 0x0F;
static constexpr uint8_t FLAG_AUTO = 0x10;
static constexpr uint8_t FLAG_BOOST = 0x40;

// The thresholds come first, in the order of their C_CONFIGURATION bytes
enum class NumberSetting : uint8_t {
  NUMBER_SETTING_HUMIDITY_THRESHOLD,
  NUMBER_SETTING_LUMINOSITY_THRESHOLD,
  NUMBER_SETTING_VOC_THRESHOLD,
  NUMBER_SETTING_TEMPERATURE_OFFSET,
  NUMBER_SETTING_HUMIDITY_OFFSET,
  NUMBER_SETTING_COUNT,
};
enum class SwitchSetting : uint8_t {
  SWITCH_SETTING_HUMIDITY_ADVANCED,
  SWITCH_SETTING_VOC_ADVANCED,
  SWITCH_SETTING_COUNT
};
enum class SelectSetting : uint8_t { SELECT_SETTING_SEASON, SELECT_SETTING_FREE_COOLING, SELECT_SETTING_COUNT };

#ifdef USE_ECOCOMFORT2_SETTINGS
/// One settings characteristic. Only one read is in flight at a time; a write sent meanwhile makes its answer stale.
struct SettingsChar {
  uint32_t sent_ms{0};
  uint16_t handle{0};
  bool needed : 1 {false};  // An entity uses this characteristic
  bool known : 1 {false};   // Read since connecting
  bool in_flight : 1 {false};
  bool dirty : 1 {false};  // A write followed the read in flight, so its answer is stale
};
#endif

/** Hub component for Ecocomfort 2.0 VMC via BLE. */
class Ecocomfort2Hub : public esphome::ble_client::BLEClientNode, public PollingComponent {
#ifdef USE_SENSOR
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(voc)
  SUB_SENSOR(direction)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(connected)
  SUB_BINARY_SENSOR(boost)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(firmware)
#endif

 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

#ifdef ECOCOMFORT2_CHILD_COUNT
  void register_child(Ecocomfort2Client *obj);
#endif
#ifdef USE_TIME
  /// Keeps the unit's clock in step with this time source while connected.
  void set_time(time::RealTimeClock *time) { this->time_ = time; }
#endif
#ifdef USE_ECOCOMFORT2_SETTINGS
  // The writes below publish every entity they affect with the value sent; a refused change publishes nothing
#ifdef USE_NUMBER
  void set_number(NumberSetting setting, number::Number *number) {
    this->numbers_[static_cast<uint8_t>(setting)] = number;
    (setting < NumberSetting::NUMBER_SETTING_TEMPERATURE_OFFSET ? this->config_char_ : this->advanced_char_).needed =
        true;
  }
  void write_number(NumberSetting setting, float value);
#endif
#ifdef USE_SWITCH
  void set_switch(SwitchSetting setting, switch_::Switch *sw) {
    this->switches_[static_cast<uint8_t>(setting)] = sw;
    this->config_char_.needed = true;
  }
  void write_switch(SwitchSetting setting, bool state);
#endif
#ifdef USE_SELECT
  void set_select(SelectSetting setting, select::Select *select) {
    this->selects_[static_cast<uint8_t>(setting)] = select;
    this->config_char_.needed = true;
  }
  void write_select(SelectSetting setting, size_t index);
#endif
#endif  // USE_ECOCOMFORT2_SETTINGS

  /// True once the link is established and paired.
  bool is_ready() const { return this->node_state == espbt::ClientState::ESTABLISHED && this->parent_->is_paired(); }

  /// Write C_OPER (mode byte, speed/flags byte) and read it back shortly after.
  void write_oper(uint8_t mode, uint8_t speed);

  /// Raw C_OPER values from the last readback.
  uint8_t get_mode() const { return this->mode_; }
  uint8_t get_speed() const { return this->speed_; }

 protected:
  static constexpr uint32_t AUTH_TIMEOUT_ID = 0;
  static constexpr uint32_t READBACK_TIMEOUT_ID = 1;
  static constexpr uint32_t AUTH_TIMEOUT_MS = 10000;
  static constexpr uint32_t READBACK_DELAY_MS = 300;

  void on_paired_();
  void set_connected_(bool connected);
  void publish_unknown_();
  bool read_char_(uint16_t handle);
  bool write_char_(uint16_t handle, uint8_t *data, uint16_t len);
  bool read_ok_(const esp_ble_gattc_cb_param_t *param);
  void on_oper_(const uint8_t *data, uint16_t len);
  void on_state_(const uint8_t *data, uint16_t len);
  void on_info_(const uint8_t *data, uint16_t len);
#ifdef USE_TIME
  void sync_clock_();
#endif
#ifdef USE_ECOCOMFORT2_SETTINGS
  void read_settings_();
  void request_settings_(SettingsChar &settings);
  bool settings_answer_current_(SettingsChar &settings);
  bool settings_known_(const SettingsChar &settings);
  void on_config_(const uint8_t *data, uint16_t len);
  void on_advanced_(const uint8_t *data, uint16_t len);
  /// Write one C_CONFIGURATION byte, leaving the others as they are, and keep `stored` as its local copy.
  void write_config_(uint8_t index, uint8_t value, uint8_t stored);
  void write_offsets_(int16_t temperature, int16_t humidity);
  bool write_settings_(SettingsChar &settings, uint8_t *data, uint16_t len);
  void publish_config_();
  void publish_offsets_();
#endif

#ifdef ECOCOMFORT2_CHILD_COUNT
  StaticVector<Ecocomfort2Client *, ECOCOMFORT2_CHILD_COUNT> children_;
#endif
#ifdef USE_TIME
  time::RealTimeClock *time_{nullptr};
#endif
  uint16_t char_handle_oper_{0};
  uint16_t char_handle_state_{0};
  uint16_t char_handle_info_{0};
  uint16_t char_handle_clock_{0};
  uint8_t mode_{OPER_OFF};
  uint8_t speed_{SPEED_OFF};
#ifdef USE_ECOCOMFORT2_SETTINGS
#ifdef USE_NUMBER
  number::Number *numbers_[static_cast<uint8_t>(NumberSetting::NUMBER_SETTING_COUNT)]{};
#endif
#ifdef USE_SWITCH
  switch_::Switch *switches_[static_cast<uint8_t>(SwitchSetting::SWITCH_SETTING_COUNT)]{};
#endif
#ifdef USE_SELECT
  select::Select *selects_[static_cast<uint8_t>(SelectSetting::SELECT_SETTING_COUNT)]{};
#endif
  SettingsChar config_char_;
  SettingsChar advanced_char_;
  // C_CONFIGURATION bytes 0-4 as last read or written: role, three thresholds, season and free cooling
  uint8_t config_bytes_[5]{};
  // C_ADVANCED_SETTINGS temperature and humidity offsets, in hundredths
  int16_t offsets_[2]{};
#endif  // USE_ECOCOMFORT2_SETTINGS
};

}  // namespace esphome::ecocomfort2

#endif  // USE_ESP32
