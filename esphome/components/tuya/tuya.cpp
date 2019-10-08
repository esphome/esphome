#include "tuya.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tuya {

static const uint8_t TUYA_CMD_HEARTBEAT = 0;
static const uint8_t TUYA_CMD_QUERY_PRODUCT = 1;
static const uint8_t TUYA_CMD_MCU_CONF = 2;
static const uint8_t TUYA_CMD_WIFI_STATE = 3;
static const uint8_t TUYA_CMD_WIFI_RESET = 4;
static const uint8_t TUYA_CMD_WIFI_SELECT = 5;
static const uint8_t TUYA_CMD_SET_DP = 6;
static const uint8_t TUYA_CMD_STATE = 7;
static const uint8_t TUYA_CMD_QUERY_STATE = 8;

static const uint8_t TUYA_TYPE_BOOL = 1;
static const uint8_t TUYA_TYPE_INT = 2;

static const char *TAG = "tuya";

/* Ask the MCU for a status update in order to find out
 * what devices are available */
void Tuya::setup() {
  uint32_t last = millis();
  uint8_t c;

  this->send_command_(TUYA_CMD_MCU_CONF);
  // there is no end of list indicator so wait until no more data is sent
  while (millis() - last < 50) {
    if (this->read_byte(&c)) {
      last = millis();
      this->handle_char_(c);
    }
  }
  in_setup_ = false;
  c = 3;
  this->send_command_(TUYA_CMD_WIFI_STATE, 1, &c);  // set wifi state LED on
}

void Tuya::loop() {
  uint32_t now = millis();
  if (now - last_hb_ > 1000) {
    last_hb_ = now;
    this->send_command_(TUYA_CMD_HEARTBEAT);
  }
  while (this->available()) {
    uint8_t c;
    if (!this->read_byte(&c))
      return;
    ESP_LOGVV(TAG, "received char %02x", c);
    this->handle_char_(c);
  }
}

void Tuya::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya:");
  if ((gpio_status_ != -1) || (gpio_reset_ != -1))
    ESP_LOGCONFIG(TAG, "  GPIO MCU configuration not supported!");
  for (auto &info : dp_info_) {
    if (info.type == TUYA_TYPE_BOOL)
      ESP_LOGCONFIG(TAG, "  %d: switch", info.id);
    else if (info.type == TUYA_TYPE_INT)
      ESP_LOGCONFIG(TAG, "  %d: int value", info.id);
    else
      ESP_LOGCONFIG(TAG, "  %d: unknown", info.id);
  }
}

void Tuya::register_listener(int dpid, TuyaListener *listener) {
  for (auto &info : dp_info_)
    if (info.id == dpid)
      info.listener = listener;
}

char *buffer_to_string(int count, uint8_t *buffer) {
  static char RET[80];
  for (int i = 0; i < count; i++)
    sprintf(RET + i * 3, "%02x ", buffer[i]);
  return RET;
}

void Tuya::handle_char_(int c) {
  static int STATE = 0;
  static uint8_t BUFFER[20];
  static int BUFPTR = 0;
  static uint8_t CHECKSUM;
  static uint8_t VERSION, COMMAND, COUNT;

  switch (STATE) {
    case 0:
      if (c == 0x55)
        STATE = 1;
      break;
    case 1:
      if (c == 0xaa)
        STATE = 2;
      else
        STATE = 0;
      break;
    case 2:
      VERSION = c;
      CHECKSUM = 0xff + c;  // 0x55 + 0xaa = 0xff
      STATE = 3;
      break;
    case 3:
      COMMAND = c;
      CHECKSUM += c;
      STATE = 4;
      break;
    case 4:
      COUNT = c;
      CHECKSUM += c;
      STATE = 5;
      break;
    case 5:
      COUNT = (COUNT << 8) + c;
      if (COUNT > 20) {
        ESP_LOGE(TAG, "received response with %d bytes of data", COUNT);
        STATE = 0;
        break;
      }
      CHECKSUM += c;
      BUFPTR = 0;
      if (COUNT > 0)
        STATE = 6;
      else
        STATE = 7;
      break;
    case 6:
      BUFFER[BUFPTR++] = c;
      CHECKSUM += c;
      if (BUFPTR == COUNT)
        STATE = 7;
      break;
    case 7:
      STATE = 0;
      ESP_LOGV(TAG, "command %d version %d count %d data [%s] checksum %d %d", COMMAND, VERSION, COUNT,
               buffer_to_string(COUNT, BUFFER), CHECKSUM, c);
      if (CHECKSUM != c)
        ESP_LOGE(TAG, "checksum error");
      else
        this->handle_command_(COMMAND, VERSION, COUNT, BUFFER);
      break;
  }
}

void Tuya::handle_command_(uint8_t command, uint8_t version, int count, uint8_t *buffer) {
  int id;
  int type;
  uint32_t value;

  switch (command) {
    case TUYA_CMD_HEARTBEAT:
      if (buffer[0] == 0) {
        ESP_LOGI(TAG, "MCU restarted");
        this->send_command_(TUYA_CMD_QUERY_STATE);
      }
      break;
    case TUYA_CMD_QUERY_PRODUCT:
      ESP_LOGI(TAG, "product code: %s", buffer);
      break;
    case TUYA_CMD_MCU_CONF:
      if (count != 0) {
        gpio_status_ = buffer[0];
        gpio_reset_ = buffer[1];
      }
      this->send_command_(TUYA_CMD_QUERY_STATE);
      break;
    case TUYA_CMD_WIFI_STATE:
      break;
    case TUYA_CMD_WIFI_RESET:
      ESP_LOGE(TAG, "TUYA_CMD_WIFI_RESET is not handled");
      break;
    case TUYA_CMD_WIFI_SELECT:
      ESP_LOGE(TAG, "TUYA_CMD_WIFI_SELECT is not handled");
      break;
    case TUYA_CMD_SET_DP:
      break;
    case TUYA_CMD_STATE:
      id = buffer[0];
      type = buffer[1];
      value = 0;
      if (type == TUYA_TYPE_BOOL)
        value = buffer[4];
      else if (type == TUYA_TYPE_INT)
        value = (buffer[4] << 24) + (buffer[5] << 16) + (buffer[6] << 8) + buffer[7];
      if (in_setup_) {
        dp_info_.push_back({id, type, value, nullptr});
        break;
      }
      ESP_LOGV(TAG, "dp %d update to %d", id, value);
      for (auto &info : dp_info_) {
        if (info.id == id) {
          if (info.value != value) {
            info.value = value;
            if (info.listener != nullptr)
              info.listener->dp_update(id, value);
          }
        }
      }
      break;
    case TUYA_CMD_QUERY_STATE:
      break;
    default:
      ESP_LOGE(TAG, "invalid command (%02x) received", command);
  }
}

void Tuya::send_command_(uint8_t command, int count, uint8_t *buffer) {
  uint8_t checksum;

  this->write_byte(0x55);
  this->write_byte(0xaa);
  this->write_byte(0x00);  // version 0
  this->write_byte(command);
  this->write_byte(0x00);
  this->write_byte(count);
  checksum = 0xff + command + count;  // 0x55 + 0xaa = 0xff
  for (int i = 0; i < count; i++) {
    this->write_byte(buffer[i]);
    checksum += buffer[i];
  }
  this->write_byte(checksum);
}

void Tuya::set_dp_value(int dpid, uint32_t value) {
  uint8_t buffer[8];
  for (auto &info : dp_info_) {
    if (info.id == dpid) {
      int count = 4;
      buffer[0] = dpid;
      buffer[1] = info.type;
      buffer[2] = 0;  // high size
      if (info.type == TUYA_TYPE_BOOL) {
        buffer[3] = 1;
        buffer[4] = value;
        count++;
      } else if (info.type == TUYA_TYPE_INT) {
        buffer[3] = 4;
        buffer[4] = value >> 24;
        buffer[5] = value >> 16;
        buffer[6] = value >> 8;
        buffer[7] = value;
        count += 4;
      }
      send_command_(TUYA_CMD_SET_DP, count, buffer);
      break;
    }
  }
}

uint32_t Tuya::get_dp_value(int dpid) {
  for (auto &info : dp_info_)
    if (info.id == dpid)
      return info.value;
  return 0;
}

}  // namespace tuya
}  // namespace esphome
