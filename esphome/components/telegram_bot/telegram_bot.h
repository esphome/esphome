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
  std::string id;
  std::string text;
  std::string chat_id;
  std::string chat_title;
  std::string from_id;
  std::string from_name;
  std::string date;
  std::string type;
  int update_id;
};

struct KeyboardButton {
  std::string text;
  std::string url;
  std::string callback_data;
};

// TelegramBotComponent
class TelegramBotComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void set_token(std::string token) { this->token_ = token; }
  void add_allowed_chat_id(std::string chat_id) { this->allowed_chat_ids_.push_back(chat_id); }
  void get_updates(long offset, const std::function<void(JsonObject &)> &callback);
  bool is_chat_allowed(std::string chat_id);
  void send_message(std::string chat_id, std::string message, std::list<KeyboardButton> inline_keyboard);
  void send_message(std::string chat_id, std::string message);
  void answer_callback_query(std::string callback_query_id, std::string message);

 protected:
  std::string token_;
  std::list<std::string> allowed_chat_ids_{};
  http_request::HttpRequestComponent *request_;
  DynamicJsonBuffer json_buffer_;
  void make_request_(const char *method, std::string body, const std::function<void(JsonObject &)> &callback);
};

// TelegramBotMessageUpdater
class TelegramBotMessageUpdater : public PollingComponent {
 public:
  explicit TelegramBotMessageUpdater(TelegramBotComponent *parent) : PollingComponent() { this->parent_ = parent; };
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void update() override;
  void schedule_update();
  void add_on_message_callback(std::function<void(Message)> &&callback);

 protected:
  TelegramBotComponent *parent_{nullptr};
  long last_message_id_ = -6;  // Get last 5 unread messages one by one
  CallbackManager<void(Message)> message_callback_{};
  Message process_message_(JsonObject &result);
};

// TelegramBotMessageTrigger
class TelegramBotMessageTrigger : public Trigger<Message> {
 public:
  explicit TelegramBotMessageTrigger(TelegramBotMessageUpdater *parent) {
    parent->add_on_message_callback([this](Message message) {
      bool type_allowed = this->type_.empty() || this->type_ == message.type;
      bool message_allowed = this->message_.empty() || this->message_ == message.text;
      if (message_allowed && type_allowed) {
        this->trigger(message);
      }
    });
  }
  void set_message(std::string message) { this->message_ = message; }
  void set_type(std::string type) { this->type_ = type; }

 protected:
  std::string message_;
  std::string type_;
};

// TelegramBotSendAction
template<typename... Ts> class TelegramBotSendAction : public Action<Ts...> {
 public:
  TelegramBotSendAction(TelegramBotComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, message)

  void add_chat_id(std::string chat_id) { this->chat_ids_.push_back(chat_id); }

  void add_keyboard_button(const char *text, const char *url, const char *callback_data) {
    KeyboardButton button;
    button.text = text;
    button.url = url;
    button.callback_data = callback_data;
    this->inline_keyboard_.push_back(button);
  }

  void play(Ts... x) override {
    for (const std::string &chat_id : this->chat_ids_) {
      this->parent_->send_message(chat_id, this->message_.value(x...), this->inline_keyboard_);
    }
  }

 protected:
  TelegramBotComponent *parent_;
  std::list<std::string> chat_ids_{};
  std::list<KeyboardButton> inline_keyboard_{};
};

// TelegramBotAnswerCallbackAction
template<typename... Ts> class TelegramBotAnswerCallbackAction : public Action<Ts...> {
 public:
  TelegramBotAnswerCallbackAction(TelegramBotComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, callback_id)
  TEMPLATABLE_VALUE(std::string, message)

  void play(Ts... x) override {
    this->parent_->answer_callback_query(this->callback_id_.value(x...), this->message_.value(x...));
  }

 protected:
  TelegramBotComponent *parent_;
};

}  // namespace telegram_bot
}  // namespace esphome
