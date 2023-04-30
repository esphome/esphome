#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"

#include <vector>

namespace esphome {
namespace fingerprint_grow {

static const uint16_t START_CODE = 0xEF01;

static const uint16_t ENROLLMENT_SLOT_UNUSED = 0xFFFF;

enum GrowPacketType {
  COMMAND = 0x01,
  DATA = 0x02,
  ACK = 0x07,
  END_DATA = 0x08,
};

enum GrowCommand {
  GET_IMAGE = 0x01,
  IMAGE_2_TZ = 0x02,
  SEARCH = 0x04,
  REG_MODEL = 0x05,
  STORE = 0x06,
  LOAD = 0x07,
  UPLOAD = 0x08,
  DELETE = 0x0C,
  EMPTY = 0x0D,
  READ_SYS_PARAM = 0x0F,
  SET_PASSWORD = 0x12,
  VERIFY_PASSWORD = 0x13,
  HI_SPEED_SEARCH = 0x1B,
  TEMPLATE_COUNT = 0x1D,
  AURA_CONFIG = 0x35,
  LED_ON = 0x50,
  LED_OFF = 0x51,
};

enum GrowResponse {
  OK = 0x00,
  PACKET_RCV_ERR = 0x01,
  NO_FINGER = 0x02,
  IMAGE_FAIL = 0x03,
  IMAGE_MESS = 0x06,
  FEATURE_FAIL = 0x07,
  NO_MATCH = 0x08,
  NOT_FOUND = 0x09,
  ENROLL_MISMATCH = 0x0A,
  BAD_LOCATION = 0x0B,
  DB_RANGE_FAIL = 0x0C,
  UPLOAD_FEATURE_FAIL = 0x0D,
  PACKET_RESPONSE_FAIL = 0x0E,
  UPLOAD_FAIL = 0x0F,
  DELETE_FAIL = 0x10,
  DB_CLEAR_FAIL = 0x11,
  PASSWORD_FAIL = 0x13,
  INVALID_IMAGE = 0x15,
  FLASH_ERR = 0x18,
  INVALID_REG = 0x1A,
  BAD_PACKET = 0xFE,
  TIMEOUT = 0xFF,
};

enum GrowAuraLEDState {
  BREATHING = 0x01,
  FLASHING = 0x02,
  ALWAYS_ON = 0x03,
  ALWAYS_OFF = 0x04,
  GRADUAL_ON = 0x05,
  GRADUAL_OFF = 0x06,
};

enum GrowAuraLEDColor {
  RED = 0x01,
  BLUE = 0x02,
  PURPLE = 0x03,
  GREEN = 0x04,
  YELLOW = 0x05,
  CYAN = 0x06,
  WHITE = 0x07,
};

class FingerprintGrowComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void update() override;
  void setup() override;
  void dump_config() override;

  void set_address(uint32_t address) {
    this->address_[0] = (uint8_t) (address >> 24);
    this->address_[1] = (uint8_t) (address >> 16);
    this->address_[2] = (uint8_t) (address >> 8);
    this->address_[3] = (uint8_t) (address & 0xFF);
  }
  void set_sensing_pin(GPIOPin *sensing_pin) { this->sensing_pin_ = sensing_pin; }
  void set_password(uint32_t password) { this->password_ = password; }
  void set_new_password(uint32_t new_password) { this->new_password_ = new_password; }
  void set_fingerprint_count_sensor(sensor::Sensor *fingerprint_count_sensor) {
    this->fingerprint_count_sensor_ = fingerprint_count_sensor;
  }
  void set_status_sensor(sensor::Sensor *status_sensor) { this->status_sensor_ = status_sensor; }
  void set_capacity_sensor(sensor::Sensor *capacity_sensor) { this->capacity_sensor_ = capacity_sensor; }
  void set_security_level_sensor(sensor::Sensor *security_level_sensor) {
    this->security_level_sensor_ = security_level_sensor;
  }
  void set_last_finger_id_sensor(sensor::Sensor *last_finger_id_sensor) {
    this->last_finger_id_sensor_ = last_finger_id_sensor;
  }
  void set_last_confidence_sensor(sensor::Sensor *last_confidence_sensor) {
    this->last_confidence_sensor_ = last_confidence_sensor;
  }
  void set_enrolling_binary_sensor(binary_sensor::BinarySensor *enrolling_binary_sensor) {
    this->enrolling_binary_sensor_ = enrolling_binary_sensor;
  }
  void add_on_finger_scan_matched_callback(std::function<void(uint16_t, uint16_t)> callback) {
    this->finger_scan_matched_callback_.add(std::move(callback));
  }
  void add_on_finger_scan_unmatched_callback(std::function<void()> callback) {
    this->finger_scan_unmatched_callback_.add(std::move(callback));
  }
  void add_on_enrollment_scan_callback(std::function<void(uint8_t, uint16_t)> callback) {
    this->enrollment_scan_callback_.add(std::move(callback));
  }
  void add_on_enrollment_done_callback(std::function<void(uint16_t)> callback) {
    this->enrollment_done_callback_.add(std::move(callback));
  }

  void add_on_enrollment_failed_callback(std::function<void(uint16_t)> callback) {
    this->enrollment_failed_callback_.add(std::move(callback));
  }

  void enroll_fingerprint(uint16_t finger_id, uint8_t num_buffers);
  void finish_enrollment(uint8_t result);
  void delete_fingerprint(uint16_t finger_id);
  void delete_all_fingerprints();

  void led_control(bool state);
  void aura_led_control(uint8_t state, uint8_t speed, uint8_t color, uint8_t count);

 protected:
  void scan_and_match_();
  uint8_t scan_image_(uint8_t buffer);
  uint8_t save_fingerprint_();
  bool check_password_();
  bool set_password_();
  bool get_parameters_();
  void get_fingerprint_count_();
  uint8_t send_command_();

  std::vector<uint8_t> data_ = {};
  uint8_t address_[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  uint16_t capacity_ = 64;
  uint32_t password_ = 0x0;
  uint32_t new_password_ = -1;
  GPIOPin *sensing_pin_{nullptr};
  uint8_t enrollment_image_ = 0;
  uint16_t enrollment_slot_ = ENROLLMENT_SLOT_UNUSED;
  uint8_t enrollment_buffers_ = 5;
  bool waiting_removal_ = false;
  uint32_t last_aura_led_control_ = 0;
  uint16_t last_aura_led_duration_ = 0;
  sensor::Sensor *fingerprint_count_sensor_{nullptr};
  sensor::Sensor *status_sensor_{nullptr};
  sensor::Sensor *capacity_sensor_{nullptr};
  sensor::Sensor *security_level_sensor_{nullptr};
  sensor::Sensor *last_finger_id_sensor_{nullptr};
  sensor::Sensor *last_confidence_sensor_{nullptr};
  binary_sensor::BinarySensor *enrolling_binary_sensor_{nullptr};
  CallbackManager<void(uint16_t, uint16_t)> finger_scan_matched_callback_;
  CallbackManager<void()> finger_scan_unmatched_callback_;
  CallbackManager<void(uint8_t, uint16_t)> enrollment_scan_callback_;
  CallbackManager<void(uint16_t)> enrollment_done_callback_;
  CallbackManager<void(uint16_t)> enrollment_failed_callback_;
};

class FingerScanMatchedTrigger : public Trigger<uint16_t, uint16_t> {
 public:
  explicit FingerScanMatchedTrigger(FingerprintGrowComponent *parent) {
    parent->add_on_finger_scan_matched_callback(
        [this](uint16_t finger_id, uint16_t confidence) { this->trigger(finger_id, confidence); });
  }
};

class FingerScanUnmatchedTrigger : public Trigger<> {
 public:
  explicit FingerScanUnmatchedTrigger(FingerprintGrowComponent *parent) {
    parent->add_on_finger_scan_unmatched_callback([this]() { this->trigger(); });
  }
};

class EnrollmentScanTrigger : public Trigger<uint8_t, uint16_t> {
 public:
  explicit EnrollmentScanTrigger(FingerprintGrowComponent *parent) {
    parent->add_on_enrollment_scan_callback(
        [this](uint8_t scan_num, uint16_t finger_id) { this->trigger(scan_num, finger_id); });
  }
};

class EnrollmentDoneTrigger : public Trigger<uint16_t> {
 public:
  explicit EnrollmentDoneTrigger(FingerprintGrowComponent *parent) {
    parent->add_on_enrollment_done_callback([this](uint16_t finger_id) { this->trigger(finger_id); });
  }
};

class EnrollmentFailedTrigger : public Trigger<uint16_t> {
 public:
  explicit EnrollmentFailedTrigger(FingerprintGrowComponent *parent) {
    parent->add_on_enrollment_failed_callback([this](uint16_t finger_id) { this->trigger(finger_id); });
  }
};

template<typename... Ts> class EnrollmentAction : public Action<Ts...>, public Parented<FingerprintGrowComponent> {
 public:
  TEMPLATABLE_VALUE(uint16_t, finger_id)
  TEMPLATABLE_VALUE(uint8_t, num_scans)

  void play(Ts... x) override {
    auto finger_id = this->finger_id_.value(x...);
    auto num_scans = this->num_scans_.value(x...);
    if (num_scans) {
      this->parent_->enroll_fingerprint(finger_id, num_scans);
    } else {
      this->parent_->enroll_fingerprint(finger_id, 2);
    }
  }
};

template<typename... Ts>
class CancelEnrollmentAction : public Action<Ts...>, public Parented<FingerprintGrowComponent> {
 public:
  void play(Ts... x) override { this->parent_->finish_enrollment(1); }
};

template<typename... Ts> class DeleteAction : public Action<Ts...>, public Parented<FingerprintGrowComponent> {
 public:
  TEMPLATABLE_VALUE(uint16_t, finger_id)

  void play(Ts... x) override {
    auto finger_id = this->finger_id_.value(x...);
    this->parent_->delete_fingerprint(finger_id);
  }
};

template<typename... Ts> class DeleteAllAction : public Action<Ts...>, public Parented<FingerprintGrowComponent> {
 public:
  void play(Ts... x) override { this->parent_->delete_all_fingerprints(); }
};

template<typename... Ts> class LEDControlAction : public Action<Ts...>, public Parented<FingerprintGrowComponent> {
 public:
  TEMPLATABLE_VALUE(bool, state)

  void play(Ts... x) override {
    auto state = this->state_.value(x...);
    this->parent_->led_control(state);
  }
};

template<typename... Ts> class AuraLEDControlAction : public Action<Ts...>, public Parented<FingerprintGrowComponent> {
 public:
  TEMPLATABLE_VALUE(uint8_t, state)
  TEMPLATABLE_VALUE(uint8_t, speed)
  TEMPLATABLE_VALUE(uint8_t, color)
  TEMPLATABLE_VALUE(uint8_t, count)

  void play(Ts... x) override {
    auto state = this->state_.value(x...);
    auto speed = this->speed_.value(x...);
    auto color = this->color_.value(x...);
    auto count = this->count_.value(x...);

    this->parent_->aura_led_control(state, speed, color, count);
  }
};

}  // namespace fingerprint_grow
}  // namespace esphome
