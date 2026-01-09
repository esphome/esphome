#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"
#include <string>
#include <stdint.h>

#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace dfrobot_c4002 {

/*  */
class C4002Listener {
 public:
  virtual void on_movement_distance(float distance){};
  virtual void on_movement_speed(float speed){};
  virtual void on_movement_direction(float direction){};
  virtual void on_existing_distance(float distance){};
  virtual void on_target_status(uint8_t state){};
};

static const uint8_t TIME_OUT = 0x64;  ///< time out

static const uint8_t C4002_FRAME_HEADER1 = 0xFA;  ///< frame header1
static const uint8_t C4002_FRAME_HEADER2 = 0xF5;  ///< frame header2
static const uint8_t C4002_FRAME_HEADER3 = 0xAA;  ///< frame header3
static const uint8_t C4002_FRAME_HEADER4 = 0xA5;  ///< frame header4

static const uint8_t FRAME_TYPE_WRITE_REQUSET = 0x00;
static const uint8_t FRAME_TYPE_READ_REQUSET = 0x01;
static const uint8_t FRAME_TYPE_WRITE_RESPOND = 0x02;
static const uint8_t FRAME_TYPE_READ_RESPOND = 0x03;
static const uint8_t FRAME_TYPE_NOTIFICATION = 0x04;
static const uint8_t FRAME_ERROR = 0xFF;

static const uint8_t CMD_SET_LED_MODE = 0xA1;
static const uint8_t CMD_CONFIG_OUT_MODE = 0xA0;
static const uint8_t CMD_ENVIRNMENT_CALIBRATION = 0x60;
static const uint8_t CMD_RESTART = 0x00;
static const uint8_t CMD_SET_DETECT_RANGE = 0x86;
static const uint8_t CMD_FACTORY_RESET = 0x80;
static const uint8_t CMD_SET_REPORT_PERIOD = 0x83;
static const uint8_t CMD_SET_LIGHT_THRESHOLD = 0x88;
static const uint8_t CMD_SET_DISTANCE_DOOR = 0x62;
static const uint8_t CMD_GET_VERSION = 0x82;
static const uint8_t CMD_GET_AND_SET_RESOLUTION_MODE = 0x66;
static const uint8_t CMD_SET_DISTANCE_DOOR_THRESHOLD = 0x63;
static const uint8_t CMD_SET_BAUDRATE = 0x21;
static const uint8_t CMD_TARGET_DISAPPEAR_DELAY_TIME = 0x84;
static const uint8_t CMD_FACTORY_RESET_USER = 0x02;

static const uint8_t NOTE_RESULT_CMD = 0x60;
static const uint8_t NOTE_ENVIRNMENT_CALIBRATION_CMD = 0x03;

static const uint8_t SOFTWARE_VERSION = 0x01;
static const uint8_t HARDWARE_VERSION = 0x00;
static const int DOOR_COUNT = 15;

/**
 * @enum ResolutionMode
 * @brief Resolution mode
 */
enum ResolutionMode { RESOLUTION_80CM = 0x00, RESOLUTION_20CM = 0x01 };

/**
 * @enum DistanceDoorType
 * @brief Distance door type
 */
enum DistanceDoorType { MOVE_DIST_DOOR = 0x00, EXIST_DIST_DOOR = 0x01 };

/**
 * @enum ResponseCode
 * @brief Response code
 */
enum ResponseCode {
  READ_AND_WRITE_REQ = 0x00, /*read and write request       */
  SUCCEED = 0x01,
  CMD_ERR = 0x02,            /* The CMD does not exist      */
  AUTHENTICATION_ERR = 0x03, /* Authentication error        */
  RESOURCES_BUSY = 0x04,     /* Resources are busy          */
  PARAMS_ERR = 0x05,         /* The parameters are illegal  */
  DATALEN_ERR = 0x06,        /* Abnormal data length        */
  INTERNAL_ERR = 0x07        /* internal error              */
};

/**
 * @enum MoveDirection
 * @brief The direction of the movement
 */
enum MoveDirection { AWAY = 0, STAY = 1, NEAR = 2 };

/**
 * @enum OutMode
 * @brief Output mode
 */
enum OutMode {
  OUT_MODE1 = 0x01, /* Only when motion is detected will a high level be output */
  OUT_MODE2 = 0x02, /* A high level is output only when its presence is detected */
  OUT_MODE3 = 0x03, /* A high level only appears when movement or presence is detected */
  OUT_MODEX = 0xFF  /* reserved                          */
};

/**
 * @enum TargetState
 * @brief The state of the target
 */
enum TargetState { NO_BODY = 0, EXIST = 1, MOVE = 2, TARGET_ERROR = 255 };

/**
 * @enum LedMode
 * @brief The operation led mode
 */
enum LedMode { LED_OFF = 0x00, LED_ON = 0x01, LED_KEEP = 0xFF };

/**
 * @enum NoteTypes
 * @brief The type of the notification message
 */
enum NoteType {
  NO_NOTE = 0x00,
  NOTE_INFO_RESULT = 0x01,
  NOTE_INFO_CALIBRATION = 0x02,
};

/**
 * @struct DetectResult
 * @brief The detection result
 */
struct DetectResult {
  uint8_t targetStatus;
  uint16_t light;
  uint32_t existDistIndex;
  uint16_t existCountDown;
  uint16_t existTargetDist;
  uint8_t existTargetEnery;
  uint16_t moveTargetDist;
  int16_t moveTargetSpeed;
  uint8_t moveTargetEnery;
  uint8_t moveTargetDirect;
};

using DetectRet = DetectResult;

/**
 * @struct DetectHead
 * @brief The data header of the received package
 */
struct DataHeader {
  uint8_t cmd;
  uint8_t respCode;
  uint16_t dataLen;
};
using DetectHead = DataHeader;

/**
 * @struct RecvPack
 * @brief The received package
 */
struct RecvPack {
  DetectHead dataHeader;
  uint8_t data[50];
  uint8_t packType;
  ResponseCode resPonCode;
};

using RecvPck = RecvPack;

/**
 * @struct ExistTarget
 * @brief The movement target
 */
struct ExistTarget {
  float distance;
  uint8_t energy;
};

using ExistTgt = ExistTarget;

/**
 * @struct MoveTarget
 * @brief The movement target
 */
struct MoveTarget {
  float distance;
  float speed;
  uint8_t energy;
  MoveDirection direction;
};

using MoveTgt = MoveTarget;

/**
 *  @struct ReturnResult
 *  @brief The detection result and environment calibration information
 */
struct ReturnResult {
  NoteType noteType;
  uint16_t calibCountdown;
};

using RetResult = ReturnResult;

/**
 * @enum RangValue
 * @brief The range value of the door
 */
enum RangValue {
  AREA1_DOOR_MIN = 0,
  AREA1_DOOR_MAX = 1,
  AREA2_DOOR_MIN = 2,
  AREA2_DOOR_MAX = 3,
  AREA3_DOOR_MIN = 4,
  AREA3_DOOR_MAX = 5,
};

/**
 * @brief Main component for the DFRobot C4002 device.
 * This class handles UART communication, parsing, and publishing to
 * Home Assistant via child components (sensors, binary sensors, numbers, switches).
 */
class C4002Component : public Component, public uart::UARTDevice {
 public:
  // explicit C4002Component(uart::UARTComponent *parent = nullptr) : uart::UARTDevice(parent) {}

  /** Lifecycle hooks */
  void setup() override;
  void loop() override;

  /** UART helpers */
  void uart_clear_buffer();

  /** Debug / configuration helpers */
  void print_config();
  void setup_number();

  /** param getters and setters **/
  void update_config_param();
  void get_data();

  //** register listener **//
  void register_listener(C4002Listener *listener) { this->listeners_.push_back(listener); }

  //** / init device **//
  bool begin();

  //** param setters **//
  bool factory_reset();
  bool set_light_threshold(float threshold);
  bool set_resolution_mode(ResolutionMode mode);
  bool enable_distance_door(DistanceDoorType door_type, const uint8_t *door_data);
  bool enable_all_distance_door(uint8_t *door_data);
  bool set_detect_range(uint16_t closest, uint16_t farthest);
  void start_env_calibration(uint16_t delay_time, uint16_t cont_time);
  bool set_run_led(LedMode run_led);
  bool set_out_led(LedMode out_led);
  bool set_out_mode(OutMode out_mode);
  bool set_report_period(uint8_t period);
  bool joint_enable_door();
  bool set_target_disappear_delay(uint16_t delay_time);

  // ** param getters ** //
  void analysis_text_report();
  void get_distance_presence_threshold(DistanceDoorType door_type, uint8_t *gate_data);
  uint16_t get_target_disappear_delay();
  TargetState get_target_state();
  float get_light();
  uint32_t get_exist_dist_index();
  ExistTgt get_exist_target_info();
  MoveTgt get_move_target_info();
  bool get_resolution_mode();
  bool get_out_mode();
  bool get_detect_range();
  RetResult get_note_info_loop();
  float get_light_threshold();

  // ** data getters ** //
  int8_t restart();
  void send_pack(void *pdata, uint16_t len, uint8_t msg_type);
  RecvPck recv_pack();
  bool check_sum(const uint8_t *pdata, uint8_t len);
  uint16_t get_check_sum(const uint8_t *pdata, uint16_t len);
  size_t uart_read_raw(uint8_t *buf, size_t bufsize, uint32_t timeout_ms = 200);
  void uart_write_data(uint8_t *datas, size_t len);

#ifdef USE_SWITCH
  //** USE_SWITCH **//
  void set_run_led_switch(switch_::Switch *sw) { this->run_led_switch_ = sw; };
  void set_out_led_switch(switch_::Switch *sw) { this->out_led_switch_ = sw; };
  void set_factory_reset_switch(switch_::Switch *sw) { this->factory_reset_switch_ = sw; };
  void set_environmental_calibration_switch(switch_::Switch *sw) { this->env_calibration_switch_ = sw; };

#endif

#ifdef USE_SELECT
  //** USE_SELECT **//
  void set_operating_mode_select(select::Select *selector) { this->operating_selector_ = selector; };
  uint8_t get_out_mode_select() { return (uint8_t) this->out_mode_; };
#endif

#ifdef USE_NUMBER
  //** USE_NUMBER **//
  void set_min_range_number(number::Number *number) { this->min_range_number_ = number; }
  void set_max_range_number(number::Number *number) { this->max_range_number_ = number; }
  void set_light_threshold_number(number::Number *number) { this->light_threshold_number_ = number; }
  void set_area1_min_range_number(number::Number *number) { this->area1_min_range_number_ = number; }
  void set_area1_max_range_number(number::Number *number) { this->area1_max_range_number_ = number; }
  void set_area2_min_range_number(number::Number *number) { this->area2_min_range_number_ = number; }
  void set_area2_max_range_number(number::Number *number) { this->area2_max_range_number_ = number; }
  void set_area3_min_range_number(number::Number *number) { this->area3_min_range_number_ = number; }
  void set_area3_max_range_number(number::Number *number) { this->area3_max_range_number_ = number; }
  void set_target_disappeard_delay_time_number(number::Number *number) {
    this->target_disappeard_delay_time_number_ = number;
  }

  //** number getters **//
  float get_min_detect_range_number() { return (float) this->current_detection_range_min_; };
  float get_max_detect_range_number() { return (float) this->current_detection_range_max_; };

  //** detect range **//
  bool set_min_range(float range);
  bool set_max_range(float range);

  //** area range **//
  float get_area_range(RangValue range_value);
  void set_area_range(RangValue range_value, float range);
#endif

#ifdef USE_TEXT_SENSOR
  void set_text_sensor(text_sensor::TextSensor *ts) { this->text_sensor_ = ts; }
#endif
  void publish_text(const std::string &msg);

 protected:
  //**all data param **//
  DetectRet detect_result_;

  //** resolution mode **//
  ResolutionMode resolution_mode_ = RESOLUTION_80CM;

  //** distance door **//
  OutMode out_mode_;

  //** detect range **//
  float min_detect_range_ = 0;
  float max_detect_range_ = 11;
  float current_detection_range_min_ = 0;
  float current_detection_range_max_ = 11;

  //** area range **//
  float current_area_[6] = {0, 0, 0, 0, 0, 0};
  uint8_t enable_door_[15] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

  //** light threshold **//
  uint16_t light_threshold_;
  uint8_t reset_flag_ = 0;

#ifdef USE_SELECT
  // ** USE_SELECT **//
  select::Select *operating_selector_{nullptr};
#endif

#ifdef USE_SWITCH
  // ** USE_SWITCH **//
  switch_::Switch *run_led_switch_{nullptr};
  switch_::Switch *out_led_switch_{nullptr};
  switch_::Switch *factory_reset_switch_{nullptr};
  switch_::Switch *env_calibration_switch_{nullptr};
#endif

#ifdef USE_NUMBER
  // ** USE_NUMBER **//
  number::Number *min_range_number_{nullptr};
  number::Number *max_range_number_{nullptr};
  number::Number *light_threshold_number_{nullptr};
  number::Number *area1_min_range_number_{nullptr};
  number::Number *area1_max_range_number_{nullptr};
  number::Number *area2_min_range_number_{nullptr};
  number::Number *area2_max_range_number_{nullptr};
  number::Number *area3_min_range_number_{nullptr};
  number::Number *area3_max_range_number_{nullptr};
  number::Number *target_disappeard_delay_time_number_{nullptr};
#endif

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *text_sensor_{nullptr};

  float interval_point_[15] = {0.2, 0.8, 1.6, 2.4, 3.2, 4, 4.8, 5.6, 6.4, 7.2, 8, 8.8, 9.6, 10.4, 11.2};

#endif

  std::vector<C4002Listener *> listeners_{};
};

}  // namespace dfrobot_c4002
}  // namespace esphome
