#include "telegram_bot.h"
#include "esphome/core/log.h"

namespace esphome {
namespace telegram_bot {

static const char *TAG = "telegram_bot";

void TelegramBotComponent::setup() {
  this->request_ = new http_request::HttpRequestComponent();
  this->request_->setup();

  std::list<http_request::Header> headers;
  http_request::Header header;
  header.name = "Content-Type";
  header.value = "application/json";
  headers.push_back(header);

  this->request_->set_headers(headers);
  this->request_->set_method("POST");
  this->request_->set_useragent("ESPHome Telegram Bot");
  this->request_->set_timeout(5000);
}

void TelegramBotComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Telegram Bot:");
  std::string token = this->token_;
  if (token.length() > 35) {
    token.replace(15, 20, "********************");
  }
  ESP_LOGCONFIG(TAG, "  Token: %s", token.c_str());
}

bool TelegramBotComponent::is_chat_allowed(std::string chat_id) {
  return this->allowed_chat_ids_.empty() || std::find(this->allowed_chat_ids_.begin(), this->allowed_chat_ids_.end(),
                                                      chat_id) != this->allowed_chat_ids_.end();
}

void TelegramBotComponent::make_request_(const char *method, std::string body,
                                         const std::function<void(JsonObject &)> &callback) {
  std::string url = "https://api.telegram.org/bot" + this->token_ + "/" + to_string(method);
  this->request_->set_url(url);
  this->request_->set_force_reuse(true);
  this->request_->set_body(body);
  this->request_->send();

  if (callback != nullptr) {
    String response = this->request_->get_string();
    if (response.length() == 0) {
      ESP_LOGD(TAG, "Got empty response for method %s", method);
      callback(this->json_buffer_.createObject());
    } else if (response.length() > 2048) {
      ESP_LOGW(TAG, "Message too long, skipped");
      callback(this->json_buffer_.createObject());
    } else {
      JsonObject &root = this->json_buffer_.parseObject(response);
      callback(root);
      this->json_buffer_.clear();
    }
  }

  this->request_->close();
}

void TelegramBotComponent::get_updates(long offset, const std::function<void(JsonObject &)> &callback) {
  ESP_LOGV(TAG, "Get updates from id: %ld", offset);

  JsonObject &body = this->json_buffer_.createObject();
  body["offset"] = offset;
  body["limit"] = 1;
  JsonArray &allowed_updates = body.createNestedArray("allowed_updates");
  allowed_updates.add("message");
  allowed_updates.add("channel_post");
  allowed_updates.add("callback_query");

  this->make_request_("getUpdates", ((JsonVariant) body).as<std::string>(), callback);
  this->json_buffer_.clear();
}

void TelegramBotComponent::send_message(std::string chat_id, std::string message,
                                        std::list<KeyboardButton> inline_keyboard) {
  ESP_LOGV(TAG, "Send message to chat '%s': %s", chat_id.c_str(), message.c_str());

  JsonObject &body = this->json_buffer_.createObject();
  body["chat_id"] = chat_id;
  body["text"] = message;

  if (!inline_keyboard.empty()) {
    JsonArray &buttons =
        body.createNestedObject("reply_markup").createNestedArray("inline_keyboard").createNestedArray();
    for (const KeyboardButton &btn : inline_keyboard) {
      JsonObject &button = buttons.createNestedObject();
      button["text"] = btn.text;
      if (!btn.url.empty()) {
        button["url"] = btn.url;
      }
      if (!btn.callback_data.empty()) {
        button["callback_data"] = btn.callback_data;
      }
    }
  }

  this->make_request_("sendMessage", ((JsonVariant) body).as<std::string>(), nullptr);
  this->json_buffer_.clear();
}

void TelegramBotComponent::send_message(std::string chat_id, std::string message) {
  std::list<telegram_bot::KeyboardButton> inline_keyboard;
  this->send_message(chat_id, message, inline_keyboard);
}

void TelegramBotComponent::answer_callback_query(std::string callback_query_id, std::string message) {
  ESP_LOGV(TAG, "Send callback to '%s': %s", callback_query_id.c_str(), message.c_str());

  JsonObject &body = this->json_buffer_.createObject();
  body["callback_query_id"] = callback_query_id;
  body["text"] = message;

  this->make_request_("answerCallbackQuery", ((JsonVariant) body).as<std::string>(), nullptr);
  this->json_buffer_.clear();
}

// TelegramBotMessageUpdater
void TelegramBotMessageUpdater::update() {
  long offset = this->last_message_id_ + 1;
  if (offset == 0) {
    this->last_message_id_ = -6;
  }

  this->parent_->get_updates(offset, [this](JsonObject &root) {
    if (root.success() && root.size() > 0) {
      bool is_ok = root["ok"].as<bool>() && root.containsKey("result");
      int size = root["result"].size();
      ESP_LOGV(TAG, "Response size: %d", size);

      if (is_ok && size > 0) {
        for (int i = 0; i < size; i++) {
          Message msg = this->process_message_(root["result"][i]);
          bool chat_allowed = this->parent_->is_chat_allowed(msg.chat_id);

          if (chat_allowed) {
            ESP_LOGV(TAG, "Got message: %s", msg.text.c_str());
            this->message_callback_.call(msg);
          } else {
            ESP_LOGD(TAG, "Message from disallowed chat");
          }
        }
        this->schedule_update();
      }
    } else {
      ESP_LOGW(TAG, "Error parsing response, message skipped");
      this->last_message_id_++;
      this->schedule_update();
    }
  });
}

void TelegramBotMessageUpdater::schedule_update() {
  this->set_timeout("update", 1000, [this]() { this->update(); });
}

void TelegramBotMessageUpdater::add_on_message_callback(std::function<void(Message)> &&callback) {
  this->message_callback_.add(std::move(callback));
}

Message TelegramBotMessageUpdater::process_message_(JsonObject &result) {
  int update_id = result["update_id"];

  Message message;
  if (this->last_message_id_ == update_id) {
    return message;
  }
  this->last_message_id_ = update_id;

  message.update_id = update_id;
  message.from_id = "";
  message.from_name = "";
  message.chat_title = "";
  message.date = result["message"]["date"].as<std::string>();

  if (result.containsKey("message")) {
    JsonObject &data = result["message"];
    message.type = "message";
    message.id = data["message_id"].as<std::string>();
    message.from_id = data["from"]["id"].as<std::string>();
    message.from_name = data["from"]["first_name"].as<std::string>();
    message.text = data["text"].as<std::string>();
    message.chat_id = data["chat"]["id"].as<std::string>();
    message.chat_title = data["chat"]["title"].as<std::string>();
  } else if (result.containsKey("channel_post")) {
    JsonObject &data = result["channel_post"];
    message.type = "channel_post";
    message.id = data["message_id"].as<std::string>();
    message.text = data["text"].as<std::string>();
    message.chat_id = data["chat"]["id"].as<std::string>();
    message.chat_title = data["chat"]["title"].as<std::string>();
  } else if (result.containsKey("callback_query")) {
    JsonObject &data = result["callback_query"];
    message.type = "callback_query";
    message.id = data["id"].as<std::string>();
    message.from_id = data["from"]["id"].as<std::string>();
    message.from_name = data["from"]["first_name"].as<std::string>();
    message.text = data["data"].as<std::string>();
    message.chat_id = data["message"]["chat"]["id"].as<std::string>();
  }

  return message;
}

}  // namespace telegram_bot
}  // namespace esphome
