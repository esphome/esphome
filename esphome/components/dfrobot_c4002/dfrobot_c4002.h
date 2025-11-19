#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"
#include <string>
#include <stdint.h>
//#include "esphome/core/automation.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
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

namespace esphome {
namespace dfrobot_c4002 {

/*  */
class C4002Listener {
 public:
  virtual void on_movement_distance(float distance){};
  virtual void on_movement_speed(float speed){};
  virtual void on_movement_direction(float direction){};
  virtual void on_existing_distance(float distance){};
  virtual void on_target_state(bool presence){};
};

#define TIME_OUT 0x64       ///< time out
#define FRAME_HEADER1 0xFA  ///< frame header1
#define FRAME_HEADER2 0xF5  ///< frame header2
#define FRAME_HEADER3 0xAA  ///< frame header3
#define FRAME_HEADER4 0xA5  ///< frame header4

#define FRAME_TYPE_WRITE_REQUSET 0x00  ///< write request frame type
#define FRAME_TYPE_READ_REQUSET 0x01   ///< read request frame type
#define FRAME_TYPE_WRITE_RESPOND 0x02  ///< write respond frame type
#define FRAME_TYPE_READ_RESPOND 0x03   ///< read respond frame type
#define FRAME_TYPE_NOTIFICATION 0x04   ///< notification frame type
#define FRAME_ERROR 0xFF               ///< error frame type

#define CMD_SET_LED_MODE 0xA1                 ///< set led mode
#define CMD_CONFIG_OUT_MODE 0xA0              ///< set output mode
#define CMD_ENVIRNMENT_CALIBRATION 0x60       ///< environment calibration
#define CMD_RESTART 0x00                      ///< restart command
#define CMD_SET_DETECT_RANGE 0x86             ///< set detect sensitivity
#define CMD_FACTORY_RESET 0x80                ///< factory reset command
#define CMD_SET_REPORT_PERIOD 0x83            ///< set report period
#define CMD_SET_LIGHT_THRESHOLD 0x88          ///< set light threshold
#define CMD_SET_DISTANCE_DOOR 0x62            ///< set distance door
#define CMD_GET_VERSION 0x82                  ///< get version command
#define CMD_GET_AND_SET_RESOLUTION_MODE 0x66  ///< get resolution mode command
#define CMD_SET_DISTANCE_DOOR_THRESHOLD 0x63  ///< set distance door threshold
#define CMD_SET_BAUDRATE 0x21                 ///< set baudrate command

#define NOTE_RESULT_CMD 0x60                  ///< detection result notification command
#define NOTE_ENVIRNMENT_CALIBRATION_CMD 0x03  ///< environment calibration notification command

#define SOFTWARE_VERSION 0x01  ///< get software version
#define HARDWARE_VERSION 0x00  ///< get hardware version

/**
 * @enum ResolutionMode
 * @brief Resolution mode
 */
enum ResolutionMode{
   eResolution80Cm = 0x00, eResolution20Cm = 0x01 
  };

/**
 * @enum DistanceDoorType
 * @brief Distance door type
 */
 enum DistanceDoorType { eMoveDistDoor = 0x00, eExistDistDoor = 0x01 };

/**
 * @enum ResponseCode
 * @brief Response code
 */
enum ResponseCode {
  eReadAndWriteReq = 0x00, /*read and write request       */
  eSucceed = 0x01,
  eCmdErr = 0x02,            /* The CMD does not exist      */
  eAuthenticationErr = 0x03, /* Authentication error        */
  eResourcesBusy = 0x04,     /* Resources are busy          */
  eParamsErr = 0x05,         /* The parameters are illegal  */
  eDataLenErr = 0x06,        /* Abnormal data length        */
  eInternalErr = 0x07        /* internal error              */
};

/**
 * @enum MoveDirection
 * @brief The direction of the movement
 */
enum MoveDirection { eAway = 0, eStay = 1, eNear = 2 } ;

/**
 * @enum OutMode
 * @brief Output mode
 */
enum OutMode {
  eOutMode1 = 0x01, /* Only when motion is detected will a high level be output */
  eOutMode2 = 0x02, /* A high level is output only when its presence is detected */
  eOutMode3 = 0x03, /* A high level only appears when movement or presence is detected */
  eOutModex = 0xFF  /* reserved                          */
};

/**
 * @enum TargetState
 * @brief The state of the target
 */
enum TargetState {
  eNobody = 0,
  eExist = 1,
  eMove = 2,
  eMoveOrExist = 3,
  eMoveOrNobody = 4,
  eExistOrNobody = 5,
  ePinError = 255
} ;

/**
 * @enum LedMode
 * @brief The operation led mode
 */
enum LedMode { eLedOff = 0x00, eLedOn = 0x01, eLedKeep = 0xFF };

/**
 * @enum NoteType
 * @brief The type of the notification message
 */
enum NoteType{
  eNoNote = 0x00,
  eNoteInfoResult = 0x01,
  eNoteInfoCalibration = 0x02,
};

/**
 * @struct DetectResult
 * @brief The detection result
 */
struct DetectResult{
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
struct DataHeader{
  uint8_t cmd;
  uint8_t respCode;
  uint16_t dataLen;
};
using  DetectHead = DataHeader;

/**
 * @struct RecvPack
 * @brief The received package
 */
struct RecvPack{
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
struct ExistTarget{
  float distance;
  uint8_t energy;
} ;

  using ExistTgt = ExistTarget;

/**
 * @struct MoveTarget
 * @brief The movement target
 */
struct MoveTarget{
  float distance;
  float speed;
  uint8_t energy;
  MoveDirection direction;
} ;

using MoveTgt = MoveTarget;

/**
 *  @struct ReturnResult
 *  @brief The detection result and environment calibration information
 */
struct ReturnResult{
  NoteType noteType;
  uint16_t calibCountdown;
} ;

using RetResult = ReturnResult;

enum RangValue{
  AREA1_DOOR_MIN = 0,
  AREA1_DOOR_MAX = 1,
  AREA2_DOOR_MIN = 2,
  AREA2_DOOR_MAX = 3,
  AREA3_DOOR_MIN = 4,
  AREA3_DOOR_MAX = 5,
} ;

/**
 * @brief Main component for the DFRobot C4002 device.
 *
 * This class handles UART communication, parsing, and publishing to
 * Home Assistant via child components (sensors, binary sensors, numbers, switches).
 */
class C4002Component : public Component, public uart::UARTDevice {
 public:
  // explicit C4002Component(uart::UARTComponent *parent = nullptr) : uart::UARTDevice(parent) {}

  /** Lifecycle hooks */
  void setup() override;
  void loop() override;  // 循环中处理数据

  /** UART helpers */
  void uart_clear_buffer();

  /** Debug / configuration helpers */
  void print_config();

  void update_config_param();
  void get_data(void);

  void register_listener(C4002Listener *listener) { this->listeners_.push_back(listener); }

  bool factoryReset(void);
  bool setLightThreshold(float threshold);
  bool setResolutionMode(ResolutionMode mode);

  bool enableDistanceDoor(DistanceDoorType doorType, uint8_t *doorData);
  bool enable_all_distance_door(uint8_t *door_data);

  bool setDetectRange(uint16_t closest, uint16_t farthest);
  void startEnvCalibration(uint16_t delayTime, uint16_t contTime);
  bool setRunLed(LedMode runLed);

  //#if 0
  //************************************/
  bool setOutLed(LedMode outLed);
  bool setOutMode(OutMode outMode);

  //#endif

  //数据获取类
  TargetState getTargetState(void);
  float getLight(void);
  uint32_t getExistDistIndex(void);
  ExistTgt getExistTargetInfo(void);
  MoveTgt getMoveTargetInfo(void);

  bool begin();
  bool getResolutionMode(void);
  //#if 0
  //*****************************************/
  bool getOutMode(void);
  bool get_detect_range(void);

  //#endif

  RetResult getNotInfoLoop(void);
  bool setReportPeriod(uint8_t period);

  void sendPack(void *pdata, uint16_t len, uint8_t msgType);
  RecvPck recvPack();
  bool checkSum(uint8_t *pdata, uint8_t len);
  uint16_t getCheckSum(uint8_t *pdata, uint16_t len);

  size_t uart_read_raw(uint8_t *buf, size_t bufsize, uint32_t timeout_ms = 200);
  void uart_write_data(uint8_t *datas, size_t len);

#ifdef USE_SWITCH
  void set_run_led_switch(switch_::Switch *sw) { this->run_led_switch_ = sw; };
  void set_out_led_switch(switch_::Switch *sw) { this->out_led_switch_ = sw; };
  void set_factory_reset_switch(switch_::Switch *sw) { this->factory_reset_switch_ = sw; };
  void set_environmental_calibration_switch(switch_::Switch *sw) { this->Env_calibration_switch_ = sw; };

#endif

#ifdef USE_SELECT
  void set_operating_mode_select(select::Select *selector) { this->operating_selector_ = selector; };
  uint8_t get_out_mode_select(void) { return (uint8_t) this->_outMode; };
#endif

  void setup_number(void);
  float get_light_threshold(void);
  bool joint_enable_door(void);

#ifdef USE_NUMBER
  float get_min_detect_range_number(void) { return (float) this->min_detect_range_; };
  float get_max_detect_range_number(void) { return (float) this->max_detect_range_; };

  bool set_min_range(float range);
  bool set_max_range(float range);

  float get_area_range(RangValue range_value);
  void set_area_range(RangValue range_value, float range);

  void set_min_range_number(number::Number *number) { this->min_range_number_ = number; }
  void set_max_range_number(number::Number *number) { this->max_range_number_ = number; }
  void set_light_threshold_number(number::Number *number) { this->light_threshold_number_ = number; }
  void set_area1_min_range_number(number::Number *number) { this->area1_min_range_number_ = number; }
  void set_area1_max_range_number(number::Number *number) { this->area1_max_range_number_ = number; }
  void set_area2_min_range_number(number::Number *number) { this->area2_min_range_number_ = number; }
  void set_area2_max_range_number(number::Number *number) { this->area2_max_range_number_ = number; }
  void set_area3_min_range_number(number::Number *number) { this->area3_min_range_number_ = number; }
  void set_area3_max_range_number(number::Number *number) { this->area3_max_range_number_ = number; }

#endif

 protected:
  DetectRet _detectResult;
  ResolutionMode _resolutionMode = eResolution80Cm;

  OutMode _outMode;

  float min_detect_range_ = 0;
  float max_detect_range_ = 11;

  float current_detection_range_min_ = 0;
  float current_detection_range_max_ = 11;

  float current_area_[6] = {0, 10, 0, 10, 0, 10};
  uint8_t enable_door_[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

  uint16_t _lightThreshold;

#ifdef USE_SELECT
  select::Select *operating_selector_{nullptr};
#endif

#ifdef USE_SWITCH
  switch_::Switch *run_led_switch_{nullptr};
  switch_::Switch *out_led_switch_{nullptr};
  switch_::Switch *factory_reset_switch_{nullptr};
  switch_::Switch *Env_calibration_switch_{nullptr};
#endif

#ifdef USE_NUMBER
  number::Number *min_range_number_{nullptr};
  number::Number *max_range_number_{nullptr};
  number::Number *light_threshold_number_{nullptr};
  number::Number *area1_min_range_number_{nullptr};
  number::Number *area1_max_range_number_{nullptr};
  number::Number *area2_min_range_number_{nullptr};
  number::Number *area2_max_range_number_{nullptr};
  number::Number *area3_min_range_number_{nullptr};
  number::Number *area3_max_range_number_{nullptr};

#endif

  std::vector<C4002Listener *> listeners_{};
};

}  // namespace dfrobot_c4002
}  // namespace esphome

//开关和模式的默认读取与更新
