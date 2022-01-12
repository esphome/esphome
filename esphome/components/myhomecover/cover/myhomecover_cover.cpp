#include "myhomecover_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <ESPAsyncTCP.h>
#include "SyncClient.h"
#include <cppQueue.h>
#define  IMPLEMENTATION  FIFO
#include <ArduinoJson.h>

const int TCP_PORT = 20000;
char newcommand[200];
char receivedmsg[100];
bool alreadyconnected = false;
char tosend[30];
AsyncClient EventSession;
bool hasauthenticated = false;
bool ismh202 = false
cppQueue  messages(sizeof(Light), 100, IMPLEMENTATION);
StaticJsonBuffer<4000> jsonBuffer;
JsonObject& doc = jsonBuffer.createObject();
char msg[128];

std::string calculated_psw(int psw, const char* nonce){
  bool flag = true;
  uint num1 = 0;
  uint num2 = 0;
  uint m_1 = 0xFFFFFFFF;
  uint m_8 = 0xFFFFFFF8;
  uint m_16 = 0xFFFFFFF0;
  uint m_128 = 0xFFFFFF80;
  uint m_16777216 = 0XFF000000;
  for(int i = 0; i<strlen(nonce); i++){
    num1 = num1 & m_1;
    num2 = num2 & m_1;
    if(nonce[i]<48 || nonce[i]>57) continue;
    if(nonce[i]!=0){
      if(flag){
        num2 = psw;
      }
      flag = false;
    }
    if(nonce[i] == '1'){

      num1 = num2 & m_128;
      num1 = num1 >> 7;
      num2 = num2 << 25;
      num1 = num1 + num2;
      
    }else if(nonce[i] == '2'){

      num1 = num2 & m_16;
      num1 = num1 >> 4;
      num2 = num2 << 28;
      num1 = num1 + num2;
      
    }else if(nonce[i] == '3'){
      num1 = num2 & m_8;
      num1 = num1 >> 3;
      num2 = num2 << 29;
      num1 = num1 + num2;
      
    }else if(nonce[i] == '4'){
      num1 = num2 << 1;
      num2 = num2 >> 31;
      num1 = num1 + num2;
      
    }else if(nonce[i] == '5'){
      num1 = num2 << 5;
      num2 = num2 >> 27;
      num1 = num1 + num2;
      
    }else if(nonce[i] == '6'){
      if(flag)
          num2 = psw; //11000000111001
      num1 = num2 << 12; // 11000000111001000000000000
      num2 = num2 >> 20; //000000
      num1 = num1 + num2; // 305418240
      
    }else if(nonce[i] == '7'){
      num1 = num2 & 0xFF00;
      num1 = num1 + (( num2 & 0xFF ) << 24 );
      num1 = num1 + (( num2 & 0xFF0000 ) >> 16 );
      num2 = ( num2 & m_16777216 ) >> 8;
      num1 = num1 + num2;
      
    }else if(nonce[i] == '8'){
      num1 = num2 & 0xFFFF;
      num1 = num1 << 16;
      num1 = num1 + ( num2 >> 24 );
      num2 = num2 & 0xFF0000;
      num2 = num2 >> 8;
      num1 = num1 + num2;
      
    }else if(nonce[i] == '9'){
      num1 = ~num2;
      
    }else{
      num1 = num2;
    }
    num2 = num1;
  }
  return esphome::to_string(num1 & m_1);
}



namespace esphome {
namespace myhomecover {

static const char *const TAG = "myhomecover.cover";


void MyHomeCover::dump_config() {
  LOG_COVER("", "myhomecover Cover", this);
  ESP_LOGCONFIG(TAG, "  Channel: %s", this->channel_.c_str());
  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
}
void MyHomeCover::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->position = 0.5f;
  }
}
void MyHomeCover::loop() {
  if (this->current_operation == esphome::cover::COVER_OPERATION_IDLE)
    return;

  const uint32_t now = millis();

  // Recompute position every loop cycle
  this->recompute_position_();

  if (this->is_at_target_()) {
    if (this->target_position_ == esphome::cover::COVER_OPEN || this->target_position_ == esphome::cover::COVER_CLOSED) {
      // Don't trigger stop, let the cover stop by itself.
      this->current_operation = esphome::cover::COVER_OPERATION_IDLE;
    } else {
      this->start_direction_(esphome::cover::COVER_OPERATION_IDLE);
    }
    this->publish_state();
  }

  // Send current position every second
  if (now - this->last_publish_time_ > 1000) {
    this->publish_state(false);
    this->last_publish_time_ = now;
  }
}
float MyHomeCover::get_setup_priority() const { return setup_priority::DATA; }
esphome::cover::CoverTraits MyHomeCover::get_traits() {
  auto traits = esphome::cover::CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(false);
  traits.set_supports_tilt(false);
  return traits;
}
void MyHomeCover::control(const esphome::cover::CoverCall &call) {
  if (call.get_stop()) {
    this->start_direction_(esphome::cover::COVER_OPERATION_IDLE);
    this->publish_state();
  }
  if (call.get_toggle().has_value()) {
    if (this->current_operation != esphome::cover::COVER_OPERATION_IDLE) {
      this->start_direction_(esphome::cover::COVER_OPERATION_IDLE);
      this->publish_state();
    } else {
      if (this->position == esphome::cover::COVER_CLOSED || this->last_operation_ == esphome::cover::COVER_OPERATION_CLOSING) {
        this->target_position_ = esphome::cover::COVER_OPEN;
        this->start_direction_(esphome::cover::COVER_OPERATION_OPENING);
      } else {
        this->target_position_ = esphome::cover::COVER_CLOSED;
        this->start_direction_(esphome::cover::COVER_OPERATION_CLOSING);
      }
    }
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    if (pos == this->position) {
      // already at target
      // for covers with built in end stop, we should send the command again
      if (pos == esphome::cover::COVER_OPEN || pos == esphome::cover::COVER_CLOSED) {
        auto op = pos == esphome::cover::COVER_CLOSED ? esphome::cover::COVER_OPERATION_CLOSING : esphome::cover::COVER_OPERATION_OPENING;
        this->target_position_ = pos;
        this->start_direction_(op);
      }
    } else {
      auto op = pos < this->position ? esphome::cover::COVER_OPERATION_CLOSING : esphome::cover::COVER_OPERATION_OPENING;
      this->target_position_ = pos;
      this->start_direction_(op);
    }
  }
}
bool MyHomeCover::is_at_target_() const {
  switch (this->current_operation) {
    case esphome::cover::COVER_OPERATION_OPENING:
      return this->position >= this->target_position_;
    case esphome::cover::COVER_OPERATION_CLOSING:
      return this->position <= this->target_position_;
    case esphome::cover::COVER_OPERATION_IDLE:
    default:
      return true;
  }
}
void MyHomeCover::start_direction_(esphome::cover::CoverOperation dir) {
  if (dir == this->current_operation && dir != esphome::cover::COVER_OPERATION_IDLE)
    return;

  this->recompute_position_();
  int command = 0;
  switch (dir) {
    case esphome::cover::COVER_OPERATION_IDLE:
      command = 0;
      break;
    case esphome::cover::COVER_OPERATION_OPENING:
      this->last_operation_ = dir;
      command = 1;
      break;
    case esphome::cover::COVER_OPERATION_CLOSING:
      this->last_operation_ = dir;
      command = 2;
      break;
    default:
      return;
  }

  this->current_operation = dir;

  const uint32_t now = millis();
  this->start_dir_time_ = now;
  this->last_recompute_time_ = now;
  this->send_command_(this->channel_, command);
}

void MyHomeCover::set_channel(std::string channel) { this->channel_ = channel; }
void MyHomeCover::set_auth(bool doesit, int password){
  this->needs_auth_ = doesit;
  this->password_ = password;
}

void MyHomeCover::send_command_(std::string canal, int command){
      ESP_LOGD(TAG, "Enviando comando: *2*%d*%s##", command, canal.c_str());
      SyncClient client;
      std::string answer = "***************";
      int i = 0;
      if(!client.connect(this->ip_.c_str(), TCP_PORT)){
          return;
        }
        client.setTimeout(2);
        while(client.connected() && client.available() == 0){
            delay(1);
        }
        while(client.available()){
            delay(1);
            answer[i] = client.read();
            i++;
        }
        answer[i] = '\0';
        i=0;
        ESP_LOGD(TAG, "recebido: %s", answer.c_str());
        if(client.printf("*99*0##") > 0){
          while(client.connected() && client.available() == 0){
            delay(1);
          }
          while(client.available()){
            delay(1);
            answer[i] = client.read();
            i++;
          }
          answer[i] = '\0';
          i=0;
          ESP_LOGD(TAG, "recebido: %s", answer.c_str());
          if(this->needs_auth_){
            std::string password = calculated_psw(this->password_, answer.c_str());
            char message[32];
            sprintf(message, "*#%s##", password.c_str());
            client.printf(message);
            while(client.connected() && client.available() == 0){
              delay(1);
            }
            while(client.available()){
              delay(1);
              answer[i] = client.read();
              i++;
            }
            answer[i] = '\0';
            i=0;
            ESP_LOGD(TAG, "recebido: %s", answer.c_str());
          }
          if(client.printf("*2*%d*%s##", command, canal.c_str()) > 0){
            while(client.connected() && client.available() == 0){
              delay(2);
            }
            while(client.available()){
              delay(1);
              answer[i] = client.read();
              i++;
            }
            answer[i] = '\0';
            i=0;
            ESP_LOGD(TAG, "recebido: %s", answer.c_str());
            if(client.connected()){
              client.stop();
            }
          }
          if(client.connected()){
            client.stop();
          }
        } else {
          client.stop();
          while(client.connected()) delay(0);
        }
    }



void MyHomeCover::recompute_position_() {
  if (this->current_operation == esphome::cover::COVER_OPERATION_IDLE)
    return;

  float dir;
  float action_dur;
  switch (this->current_operation) {
    case esphome::cover::COVER_OPERATION_OPENING:
      dir = 1.0f;
      action_dur = this->open_duration_;
      break;
    case esphome::cover::COVER_OPERATION_CLOSING:
      dir = -1.0f;
      action_dur = this->close_duration_;
      break;
    default:
      return;
  }

  const uint32_t now = millis();
  this->position += dir * (now - this->last_recompute_time_) / action_dur;
  this->position = clamp(this->position, 0.0f, 1.0f);

  this->last_recompute_time_ = now;
}

}  // namespace myhomecover
}  // namespace esphome
