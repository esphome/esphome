#pragma once

#include <algorithm>
#include <list>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/http_request/http_request.h"

namespace esphome {
namespace telegram_bot {

struct Message {
  std::string text;
  std::string chat_id;
  std::string chat_title;
  std::string from_id;
  std::string from_name;
  std::string date;
  std::string type;
  int update_id = 0;
};

// TelegramBotComponent
class TelegramBotComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void set_token(const char *token) { this->token_ = token; }
  void add_chat_id(std::string chat_id) { this->chat_ids_.push_back(chat_id); }
  void get_updates(long offset, const std::function<void(JsonObject &)> &callback);
  bool is_chat_allowed(std::string chat_id);

 protected:
  const char *token_;
  std::list<std::string> chat_ids_{};
  http_request::HttpRequestComponent *request_;
  void make_request_(const char *method, std::string body, const std::function<void(JsonObject &)> &callback);
};

// TelegramBotMessageUpdater
class TelegramBotMessageUpdater : public PollingComponent {
 public:
  // TODO: 10000 в параметры
  explicit TelegramBotMessageUpdater(TelegramBotComponent *parent) : PollingComponent(10000) {
    this->parent_ = parent;
  };
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void update() override;
  void schedule_update();
  void add_on_message_callback(std::function<void(std::string)> &&callback);

 protected:
  TelegramBotComponent *parent_{nullptr};
  long last_message_id_ = -6; // Get last 5 unread messages one by one
  CallbackManager<void(std::string)> message_callback_{};
  Message process_message_(JsonObject &result);
};

// TelegramBotMessageTrigger
class TelegramBotMessageTrigger : public Trigger<std::string> {
 public:
  explicit TelegramBotMessageTrigger(TelegramBotMessageUpdater *parent) {
    parent->add_on_message_callback([this](std::string message) {
      bool message_allowed = this->message_.size() == 0 || this->message_ == message;
      if (message_allowed) {
        // TODO: В триггере в лямбду передать весь мессадж
        this->trigger(message);
      }
    });
  }
  void set_message(std::string message) { this->message_ = message; }

 protected:
  std::string message_;
};

}  // namespace telegram_bot
}  // namespace esphome
