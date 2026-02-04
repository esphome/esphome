#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"

#include <utility>
#include <vector>

namespace esphome::hlk_fm22x {

static const uint16_t START_CODE = 0xEFAA;
enum HlkFm22xCommand {
  NONE = 0x00,
  RESET = 0x10,
  GET_STATUS = 0x11,
  VERIFY = 0x12,
  ENROLL = 0x13,
  DELETE_FACE = 0x20,
  DELETE_ALL_FACES = 0x21,
  GET_ALL_FACE_IDS = 0x24,
  GET_VERSION = 0x30,
  GET_SERIAL_NUMBER = 0x93,
};

enum HlkFm22xResponseType {
  REPLY = 0x00,
  NOTE = 0x01,
  IMAGE = 0x02,
};

enum HlkFm22xNoteType {
  READY = 0x00,
  FACE_STATE = 0x01,
};

enum HlkFm22xResult {
  SUCCESS = 0x00,
  REJECTED = 0x01,
  ABORTED = 0x02,
  FAILED4_CAMERA = 0x04,
  FAILED4_UNKNOWNREASON = 0x05,
  FAILED4_INVALIDPARAM = 0x06,
  FAILED4_NOMEMORY = 0x07,
  FAILED4_UNKNOWNUSER = 0x08,
  FAILED4_MAXUSER = 0x09,
  FAILED4_FACEENROLLED = 0x0A,
  FAILED4_LIVENESSCHECK = 0x0C,
  FAILED4_TIMEOUT = 0x0D,
  FAILED4_AUTHORIZATION = 0x0E,
  FAILED4_READ_FILE = 0x13,
  FAILED4_WRITE_FILE = 0x14,
  FAILED4_NO_ENCRYPT = 0x15,
  FAILED4_NO_RGBIMAGE = 0x17,
  FAILED4_JPGPHOTO_LARGE = 0x18,
  FAILED4_JPGPHOTO_SMALL = 0x19,
};

enum HlkFm22xFaceDirection {
  FACE_DIRECTION_UNDEFINED = 0x00,
  FACE_DIRECTION_MIDDLE = 0x01,
  FACE_DIRECTION_RIGHT = 0x02,
  FACE_DIRECTION_LEFT = 0x04,
  FACE_DIRECTION_DOWN = 0x08,
  FACE_DIRECTION_UP = 0x10,
};

class HlkFm22xComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_face_count_sensor(sensor::Sensor *face_count_sensor) { this->face_count_sensor_ = face_count_sensor; }
  void set_status_sensor(sensor::Sensor *status_sensor) { this->status_sensor_ = status_sensor; }
  void set_last_face_id_sensor(sensor::Sensor *last_face_id_sensor) {
    this->last_face_id_sensor_ = last_face_id_sensor;
  }
  void set_last_face_name_text_sensor(text_sensor::TextSensor *last_face_name_text_sensor) {
    this->last_face_name_text_sensor_ = last_face_name_text_sensor;
  }
  void set_enrolling_binary_sensor(binary_sensor::BinarySensor *enrolling_binary_sensor) {
    this->enrolling_binary_sensor_ = enrolling_binary_sensor;
  }
  void set_version_text_sensor(text_sensor::TextSensor *version_text_sensor) {
    this->version_text_sensor_ = version_text_sensor;
  }
  void add_on_face_scan_matched_callback(std::function<void(int16_t, std::string)> callback) {
    this->face_scan_matched_callback_.add(std::move(callback));
  }
  void add_on_face_scan_unmatched_callback(std::function<void()> callback) {
    this->face_scan_unmatched_callback_.add(std::move(callback));
  }
  void add_on_face_scan_invalid_callback(std::function<void(uint8_t)> callback) {
    this->face_scan_invalid_callback_.add(std::move(callback));
  }
  void add_on_face_info_callback(
      std::function<void(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t)> callback) {
    this->face_info_callback_.add(std::move(callback));
  }
  void add_on_enrollment_done_callback(std::function<void(int16_t, uint8_t)> callback) {
    this->enrollment_done_callback_.add(std::move(callback));
  }
  void add_on_enrollment_failed_callback(std::function<void(uint8_t)> callback) {
    this->enrollment_failed_callback_.add(std::move(callback));
  }

  void enroll_face(const std::string &name, HlkFm22xFaceDirection direction);
  void scan_face();
  void delete_face(int16_t face_id);
  void delete_all_faces();
  void reset();

 protected:
  void get_face_count_();
  void send_command_(HlkFm22xCommand command, const uint8_t *data = nullptr, size_t size = 0);
  void recv_command_();
  void handle_note_(const std::vector<uint8_t> &data);
  void handle_reply_(const std::vector<uint8_t> &data);
  void set_enrolling_(bool enrolling);

  HlkFm22xCommand active_command_ = HlkFm22xCommand::NONE;
  uint16_t wait_cycles_ = 0;
  sensor::Sensor *face_count_sensor_{nullptr};
  sensor::Sensor *status_sensor_{nullptr};
  sensor::Sensor *last_face_id_sensor_{nullptr};
  binary_sensor::BinarySensor *enrolling_binary_sensor_{nullptr};
  text_sensor::TextSensor *last_face_name_text_sensor_{nullptr};
  text_sensor::TextSensor *version_text_sensor_{nullptr};
  CallbackManager<void(uint8_t)> face_scan_invalid_callback_;
  CallbackManager<void(int16_t, std::string)> face_scan_matched_callback_;
  CallbackManager<void()> face_scan_unmatched_callback_;
  CallbackManager<void(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t)> face_info_callback_;
  CallbackManager<void(int16_t, uint8_t)> enrollment_done_callback_;
  CallbackManager<void(uint8_t)> enrollment_failed_callback_;
};

class FaceScanMatchedTrigger : public Trigger<int16_t, std::string> {
 public:
  explicit FaceScanMatchedTrigger(HlkFm22xComponent *parent) {
    parent->add_on_face_scan_matched_callback(
        [this](int16_t face_id, const std::string &name) { this->trigger(face_id, name); });
  }
};

class FaceScanUnmatchedTrigger : public Trigger<> {
 public:
  explicit FaceScanUnmatchedTrigger(HlkFm22xComponent *parent) {
    parent->add_on_face_scan_unmatched_callback([this]() { this->trigger(); });
  }
};

class FaceScanInvalidTrigger : public Trigger<uint8_t> {
 public:
  explicit FaceScanInvalidTrigger(HlkFm22xComponent *parent) {
    parent->add_on_face_scan_invalid_callback([this](uint8_t error) { this->trigger(error); });
  }
};

class FaceInfoTrigger : public Trigger<int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t> {
 public:
  explicit FaceInfoTrigger(HlkFm22xComponent *parent) {
    parent->add_on_face_info_callback(
        [this](int16_t status, int16_t left, int16_t top, int16_t right, int16_t bottom, int16_t yaw, int16_t pitch,
               int16_t roll) { this->trigger(status, left, top, right, bottom, yaw, pitch, roll); });
  }
};

class EnrollmentDoneTrigger : public Trigger<int16_t, uint8_t> {
 public:
  explicit EnrollmentDoneTrigger(HlkFm22xComponent *parent) {
    parent->add_on_enrollment_done_callback(
        [this](int16_t face_id, uint8_t direction) { this->trigger(face_id, direction); });
  }
};

class EnrollmentFailedTrigger : public Trigger<uint8_t> {
 public:
  explicit EnrollmentFailedTrigger(HlkFm22xComponent *parent) {
    parent->add_on_enrollment_failed_callback([this](uint8_t error) { this->trigger(error); });
  }
};

template<typename... Ts> class EnrollmentAction : public Action<Ts...>, public Parented<HlkFm22xComponent> {
 public:
  TEMPLATABLE_VALUE(std::string, name)
  TEMPLATABLE_VALUE(uint8_t, direction)

  void play(const Ts &...x) override {
    auto name = this->name_.value(x...);
    auto direction = (HlkFm22xFaceDirection) this->direction_.value(x...);
    this->parent_->enroll_face(name, direction);
  }
};

template<typename... Ts> class DeleteAction : public Action<Ts...>, public Parented<HlkFm22xComponent> {
 public:
  TEMPLATABLE_VALUE(int16_t, face_id)

  void play(const Ts &...x) override {
    auto face_id = this->face_id_.value(x...);
    this->parent_->delete_face(face_id);
  }
};

template<typename... Ts> class DeleteAllAction : public Action<Ts...>, public Parented<HlkFm22xComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->delete_all_faces(); }
};

template<typename... Ts> class ScanAction : public Action<Ts...>, public Parented<HlkFm22xComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->scan_face(); }
};

template<typename... Ts> class ResetAction : public Action<Ts...>, public Parented<HlkFm22xComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->reset(); }
};

}  // namespace esphome::hlk_fm22x
