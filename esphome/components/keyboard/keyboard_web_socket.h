#pragma once
#include "esphome/core/defines.h"
#ifdef USE_WEBSERVER
#include <AsyncWebSocket.h>
#include <deque>
#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif
#include "keyboard.h"

namespace esphome {
namespace keyboard {

class KeyboardWebSocket : public AsyncWebHandler, public Component {
 public:
  KeyboardWebSocket(Keyboard *kbd);
  bool canHandle(AsyncWebServerRequest *request) override;
  void handleRequest(AsyncWebServerRequest *request) override;
  void setup() override;
  void loop() override;
  void set_key_map(const char **names, const std::pair<uint16_t, KeyboardType> *values, uint16_t size);

 protected:
  std::unique_ptr<AsyncWebSocket> web_socket_;
  Keyboard *kbd_;
  std::deque<std::function<void()>> to_schedule_;
#ifdef USE_ESP32
  SemaphoreHandle_t to_schedule_lock_;
#endif
  const char **names_{nullptr};
  const std::pair<uint16_t, KeyboardType> *values_{nullptr};
  uint16_t size_{0};
  void handle_(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data,
               size_t len);
  void schedule_(std::function<void()> &&f);
  uint16_t find_index_(const char *name);
};
}  // namespace keyboard
}  // namespace esphome
#endif
