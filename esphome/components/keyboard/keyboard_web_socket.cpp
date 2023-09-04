#include "keyboard_web_socket.h"
#ifdef USE_WEBSERVER
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace keyboard {

static const char *const TAG = "keyboard";

static constexpr char CMD_ANY_KEY_DOWN = 'd';
static constexpr char CMD_ANY_KEY_UP = 'u';
static constexpr char CMD_MEDIA_KEY_DOWN = 'm';
static constexpr char CMD_MEDIA_KEY_UP = 'n';

KeyboardWebSocket::KeyboardWebSocket(Keyboard *kbd) : kbd_(kbd) {
#ifdef USE_ESP32
  to_schedule_lock_ = xSemaphoreCreateMutex();
#endif
}

bool KeyboardWebSocket::canHandle(AsyncWebServerRequest *request) { return web_socket_->canHandle(request); }

void KeyboardWebSocket::handleRequest(AsyncWebServerRequest *request) { web_socket_->handleRequest(request); }

void KeyboardWebSocket::setup() {
  using std::placeholders::_1;
  using std::placeholders::_2;
  using std::placeholders::_3;
  using std::placeholders::_4;
  using std::placeholders::_5;
  using std::placeholders::_6;
  web_socket_ = make_unique<AsyncWebSocket>(String("/keyboard/") + kbd_->get_object_id().c_str());
  web_socket_->onEvent(std::bind(&KeyboardWebSocket::handle_, this, _1, _2, _3, _4, _5, _6));
}

void KeyboardWebSocket::loop() {
#ifdef USE_ESP32
  if (xSemaphoreTake(this->to_schedule_lock_, 0L)) {
#endif
    std::function<void()> fn;
    if (!to_schedule_.empty()) {
      // scheduler may execute things out of order which may lead to incorrect state
      // this->defer(std::move(to_schedule_.front()));
      // let's execute it directly from the loop
      fn = std::move(to_schedule_.front());
      to_schedule_.pop_front();
    }
#ifdef USE_ESP32
    xSemaphoreGive(this->to_schedule_lock_);
#endif
    if (fn) {
      fn();
    }
#ifdef USE_ESP32
  }
#endif
}

void KeyboardWebSocket::set_key_map(const char **names, const std::pair<uint16_t, KeyboardType> *values,
                                    uint16_t size) {
  names_ = names;
  values_ = values;
  size_ = size;
}

void KeyboardWebSocket::handle_(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg,
                                uint8_t *data, size_t len) {
  bool unhandled = true;
  if (type == WS_EVT_CONNECT || type == WS_EVT_DISCONNECT) {
    unhandled = false;
    ESP_LOGD(TAG, type == WS_EVT_CONNECT ? "WS_EVT_CONNECT" : "WS_EVT_DISCONNECT");
    for (KeyboardType kbd = KEYBOARD; kbd < MAX;) {
      auto call = kbd_->make_call(kbd);
      call.set_key({});
      schedule_([call]() mutable { call.perform(); });
      switch (kbd) {
        case KEYBOARD:
          kbd = MEDIA_KEYS;
          break;
        default:
          kbd = MAX;
          break;
      }
    }
  } else if (type == WS_EVT_DATA) {
    // data packet
    AwsFrameInfo *info = (AwsFrameInfo *) arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT && len >= 2) {
      unhandled = false;
      data[len] = 0;

      std::vector<uint16_t> keys;
      KeyboardType kbd = MAX;
      char cmd = ((char *) data)[0];
      if (cmd == CMD_MEDIA_KEY_DOWN || cmd == CMD_MEDIA_KEY_UP) {
        kbd = MEDIA_KEYS;
      }
      bool ok = true;
      const char *delimiters = ",";
      char *key = std::strtok(&((char *) data)[1], delimiters);
      while (key) {
        uint16_t index = find_index_(key);
        if (index < size_) {
          keys.push_back(values_[index].first);
          if (kbd != MAX && kbd != values_[index].second) {
            ESP_LOGW(TAG, "Can't mix keyboard type in single request: current '%d', next %d", kbd,
                     values_[index].second);
            ok = false;
            break;
          }
          kbd = values_[index].second;
        } else {
          optional<uint16_t> value_i = parse_number<uint16_t>(key);
          if (!value_i.has_value() || value_i.value() == 0) {
            ESP_LOGW(TAG, "Can't convert '%s' to key!", key);
            ok = false;
            break;
          }
          keys.push_back(value_i.value());
        }
        key = std::strtok(nullptr, delimiters);
      }
      if (ok) {
        auto call = kbd_->make_call(kbd);

        if (cmd == CMD_ANY_KEY_DOWN || cmd == CMD_MEDIA_KEY_DOWN) {
          call.key_down(std::move(keys));
        } else if (cmd == CMD_ANY_KEY_UP || cmd == CMD_MEDIA_KEY_UP) {
          call.key_up(std::move(keys));
        }

        this->schedule_([call]() mutable { call.perform(); });
      }
    }
  }
  if (unhandled) {
    ESP_LOGW(TAG, "ws unhandled msg: %d, len %d", type, len);
  }
}

void KeyboardWebSocket::schedule_(std::function<void()> &&f) {
#ifdef USE_ESP32
  xSemaphoreTake(this->to_schedule_lock_, portMAX_DELAY);
#endif
  to_schedule_.push_back(std::move(f));
#ifdef USE_ESP32
  xSemaphoreGive(this->to_schedule_lock_);
#endif
}

uint16_t KeyboardWebSocket::find_index_(const char *name) {
  auto compare = [](const char *l, const char *r) { return std::strcmp(l, r) < 0; };
  uint16_t index = std::lower_bound(names_, names_ + size_, name, compare) - &names_[0];
  if (index < size_ && std::strcmp(names_[index], name) == 0) {
    return index;
  }
  return size_;
}

}  // namespace keyboard
}  // namespace esphome
#endif
