#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "dynamic_lamp.h"
#include <string>
#include <cstring>
#include <string_view>
#include <vector>
#include <array>
#include <list>
#include <optional>
#include <algorithm>
#include <cinttypes>
#include <bit>
#include <sstream>
#include <iostream>

namespace esphome {
namespace dynamic_lamp {

static const char *TAG = "dynamic_lamp";

void DynamicLampComponent::setup() {
  this->begin();
}

void DynamicLampComponent::begin() {
  for (uint8_t i = 0; i < 16; i++) {
    this->active_lamps_[i] = CombinedLamp{0xff, i, false, false, false, false, "", 0.0f, {0, 0}};
    if (this->available_lamps_[i].available == true) {
      this->available_lamps_[i].lamp->set_internal(true);
      this->available_lamps_[i].lamp->setup();
    }
  }
  this->restore_lamp_settings_();
  this->restore_timers_();
}

void DynamicLampComponent::loop() {
  // return while rtc is invalid
  ESPTime now = this->rtc_->now();
  if (now.is_valid() != true) {
    return;
  }
  // handle lamp changes
  uint8_t i = 0;
  for (i = 0; i < this->lamp_count_; i++) {
    if (this->active_lamps_[i].active == true && this->active_lamps_[i].update_ == true) {
      // check if lamp is available
      if (this->available_lamps_[i].available != true || this->available_lamps_[i].lamp == nullptr) {
        ESP_LOGW(TAG, "Lamp %s is not available, ignoring!", this->available_lamps_[i].lamp_id.c_str());
        continue;
      }
      // check if lamp has output
      if (this->available_lamps_[i].lamp->get_output() == nullptr) {
        ESP_LOGW(TAG, "Lamp %s has no output defined, ignoring!", this->available_lamps_[i].lamp_id.c_str());
        continue;
      }
      // update lamp
      esphome::light::LightCall light_call = this->available_lamps_[i].lamp->make_call();
      if (this->active_lamps_[i].on_ == false) {
        light_call.set_state(false);
      }
      else {
        light_call.set_state(true);
      }
      light_call.set_brightness(this->active_lamps_[i].state_);
      light_call.perform();
      this->available_lamps_[i].lamp->publish_state();
      // save lamp state to fram
      this->fram_->write((0x0000 + (i * 28)), reinterpret_cast<unsigned char *>(&this->active_lamps_[i]), 28);
      // update outputs
      uint8_t j = 0;
      for (j = 0; j < 16; j++) {
        // ESP_LOGV(TAG, "Testing wether output %s is in use and attached to this lamp", this->available_outputs_[j].output_id.c_str());
        bool output_in_use_by_this_lamp = static_cast<bool>(this->active_lamps_[i].used_outputs[j / 8] & (1 << (j % 8)));
        if (output_in_use_by_this_lamp == true && this->available_outputs_[j].available == true && this->linked_outputs_[j].in_use == true) {
          // Update level
          float new_state;
          new_state = this->active_lamps_[i].state_;
          // ESP_LOGV(TAG, "Output %s is in use and attached to this lamp, updating state to %f", this->available_outputs_[j].output_id.c_str(), new_state);
          switch (this->linked_outputs_[j].mode) {
            case MODE_EQUAL:
              if (this->linked_outputs_[j].min_value && new_state < *this->linked_outputs_[j].min_value) {
                new_state = *this->linked_outputs_[j].min_value;
              }
              else if (this->linked_outputs_[j].max_value && new_state > *this->linked_outputs_[j].max_value) {
                new_state = *this->linked_outputs_[j].max_value;
              }
              break;
            case MODE_STATIC:
              new_state = this->linked_outputs_[j].mode_value;
              break;
            case MODE_PERCENTAGE:
              new_state = this->linked_outputs_[i].state * this->linked_outputs_[j].mode_value;
              if (this->linked_outputs_[j].min_value && new_state < *this->linked_outputs_[j].min_value) {
                new_state = *this->linked_outputs_[j].min_value;
              }
              else if (this->linked_outputs_[j].max_value && new_state > *this->linked_outputs_[j].max_value) {
                new_state = *this->linked_outputs_[j].max_value;
              }
              break;
            case MODE_FUNCTION:
              // ToDo - yet to be implemented
              ESP_LOGW(TAG, "Mode %" PRIu8 " for output %s is not implemented yet, sorry", this->linked_outputs_[j].mode, this->available_outputs_[j].output_id.c_str());
              this->status_set_warning();
              continue;
            default:
              // Unknown
              ESP_LOGW(TAG, "Unknown mode %" PRIu8 " for output %s", this->linked_outputs_[j].mode, this->available_outputs_[j].output_id.c_str());
              this->status_set_warning();
              continue;
          }
          ESP_LOGV(TAG, "Setting output %s to level %f", this->available_outputs_[j].output_id.c_str(), new_state);
          this->available_outputs_[j].output->set_level(new_state);
          this->linked_outputs_[j].state = new_state;
          // update relays
          if (this->linked_outputs_[j].linked_relays_[0] != 0 || this->linked_outputs_[j].linked_relays_[1] != 0 || this->linked_outputs_[j].linked_relays_[2] != 0 || this->linked_outputs_[j].linked_relays_[3] != 0) {
            uint8_t k = 0;
            for (k = 0; k < 32; k++) {
              bool relay_in_use = static_cast<bool>(this->linked_outputs_[j].linked_relays_[k / 8] & (1 << (k % 8)));
              if (relay_in_use == true && this->available_relays_[k].available == true && this->available_relays_[k].in_use == true) {
                if (this->active_lamps_[i].on_ == false) {
                  this->available_relays_[k].relay->turn_off();
                }
                else {
                  this->available_relays_[k].relay->turn_on();
                }
              }
            }
          }
        }
      }
      this->active_lamps_[i].update_ = false;
    }
  }

  // handle timers
  if (now.timestamp >= this->next_timer_exec_time_) {
    std::vector<uint8_t> timers_to_execute = this->get_timers_to_execute_(now);
    for (uint8_t iter = 0; iter < timers_to_execute.size(); iter++) {
      uint8_t timer_index = timers_to_execute[iter];
      std::stringstream namestream;
      DynamicLampTimer timer = this->timers_[timer_index];
      for (uint8_t namebyte = 0; namebyte < 32; namebyte++) {
        if (timer.timer_desc[namebyte] == 0) {
          break;
        }
        namestream << timer.timer_desc[namebyte];
      }
      ESP_LOGI(TAG, "Executing timer %s", namestream.str().c_str());
      for (uint8_t j = 0; j < 16; j++) {
        uint8_t current_lamp_state = timer.lamp_list[j / 8] & (1 << (j % 8));
        bool lamp_included = static_cast<bool>(current_lamp_state);
        float action_value = static_cast<float>(timer.action_value);
        if (lamp_included == true) {
          switch (timer.action) {
            case 0:
              this->active_lamps_[j].state_ = static_cast<float>((1.0f / 256.0f) * action_value);
              this->active_lamps_[j].update_ = true;
              break;
            case 1:
              this->active_lamps_[j].on_ = true;
              this->active_lamps_[j].update_ = true;
              break;
            case 2:
            this->active_lamps_[j].on_ = false;
              this->active_lamps_[j].update_ = true;
              break;
            default:
              ESP_LOGW(TAG, "currently only actions 0-2 (0=set_level, 1=turn_on, 2=turn_off) supported !");
              this->status_set_warning();
              continue;
          }
        }
      }
    }
    time_t next_active_timer_time = this->get_next_active_timer_timestamp_(now);
    if (next_active_timer_time == 0) {
      // no active timers found - check again in 24h - if timers are added/saved/remoed, this will be reset anyway
      ESPTime tomorrow = now;
      tomorrow.increment_day();
      ESPTime next_timer_time = ESPTime::from_epoch_local(tomorrow.timestamp);
      std::string datetime_format = "%d.%m.%Y %H:%M";
      std::string timestring = next_timer_time.strftime(datetime_format.c_str());
      ESP_LOGI(TAG, "setting next execution time %s", timestring.c_str());
      this->next_timer_exec_time_ = next_timer_time.timestamp;
      return;
    }
    ESPTime next_timer_time = ESPTime::from_epoch_local(next_active_timer_time);
    std::string datetime_format = "%d.%m.%Y %H:%M";
    std::string timestring = next_timer_time.strftime(datetime_format.c_str());
    ESP_LOGI(TAG, "setting next execution time %s", timestring.c_str());
    this->next_timer_exec_time_ = next_timer_time.timestamp;
    return;
  }
}

void DynamicLampComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Dynamic Lamp feature loaded");
  switch(this->save_mode_) {
    case SAVE_MODE_NONE:
      ESP_LOGCONFIG(TAG, "Save mode set to NONE");
      break;
    case SAVE_MODE_LOCAL:
      ESP_LOGCONFIG(TAG, "Save mode set to LOCAL");
      break;
    case SAVE_MODE_FRAM:
      ESP_LOGCONFIG(TAG, "Save mode set to FRAM");
      break;
    default:
      ESP_LOGCONFIG(TAG, "Currently only NONE(0), LOCAL(1) & FRAM(2) save modes supported, ignoring value %" PRIu8 " and defaulting to NONE!", this->save_mode_);
      this->save_mode_ = 0;
  }
  for (uint8_t i = 0; i < 16; i++) {
    if (this->available_outputs_[i].available == true) {
      ESP_LOGCONFIG(TAG, "Using output with id %s as output number %" PRIu8 "", this->available_outputs_[i].output_id.c_str(), i);
    }
  }
}

void DynamicLampComponent::set_save_mode(uint8_t save_mode) {
  this->save_mode_ = save_mode;
}

void DynamicLampComponent::set_lamp_level(uint8_t lamp_number, float state) {
  if (lamp_number > 15) {
    ESP_LOGW(TAG, "Lamp number %" PRIu8 " is out of range, ignoring call to set lamp level!", lamp_number);
    this->status_set_warning();
    return;
  }
  if (this->active_lamps_[lamp_number].active == true) {
    if (state == -1.0f) {
      this->active_lamps_[lamp_number].on_ = false;
    }
    else {
      this->active_lamps_[lamp_number].on_ = true;
      this->active_lamps_[lamp_number].state_ = state;
    }
    this->active_lamps_[lamp_number].update_ = true;
  }
}

void DynamicLampComponent::set_lamp_level_by_name(std::string lamp_name, float state) {
  uint8_t lamp_index = this->get_lamp_index_by_name_(lamp_name);
  if (lamp_index == 255) {
    ESP_LOGW(TAG, "Ignoring call to set lamp level for lamp %s", lamp_name.c_str());
    return;
  }
  this->active_lamps_[lamp_index].state_ = state;
  this->active_lamps_[lamp_index].update_ = true;
}

void DynamicLampComponent::add_available_output(output::FloatOutput* output, std::string output_id) {
  uint8_t output_index = 0;
  while (this->available_outputs_[output_index].available == true) {
    output_index++;
  }
  if (output_index > 15) {
    ESP_LOGW(TAG, "No more outputs available, max 16 outputs supported!");
    this->status_set_warning();
    return;
  }
  this->available_outputs_[output_index].available = true;
  this->available_outputs_[output_index].output_id = output_id;
  this->available_outputs_[output_index].output = output;
  this->linked_outputs_[output_index].output_index = output_index;
}

std::array<bool, 16> DynamicLampComponent::get_available_outputs() {
  std::array<bool, 16> bool_array;
  for (uint8_t i = 0; i < 16; i++) {
    bool_array[i] = static_cast<bool>(this->available_outputs_[i].available == true);
  }
  return bool_array;
}

std::string DynamicLampComponent::get_output_id_string(uint8_t output_index) {
  return this->available_outputs_[output_index].output_id;
}

LinkedOutput DynamicLampComponent::get_linked_output(uint8_t output_index) {
  return this->linked_outputs_[output_index];
}

void DynamicLampComponent::read_output_config_to_log() {
  for (uint8_t i = 0; i < 16; i++) {
    if (this->available_outputs_[i].available == true) {
      if (this->linked_outputs_[i].in_use == true) {
        uint8_t lamp_index = this->linked_outputs_[i].lamp_index;
        std::stringstream lamp_name;
        for (uint8_t j = 0; j < 16; j++) {
          if (this->active_lamps_[lamp_index].name[j] == 0) {
            break;
          }
          lamp_name << this->active_lamps_[lamp_index].name[j];
        }
        ESP_LOGV(TAG, "Output %s in use and attached to lamp %s, having output mode %" PRIu8 " and mode-value %f", this->available_outputs_[i].output_id.c_str(), lamp_name.str().c_str(), this->linked_outputs_[i].mode, this->linked_outputs_[i].mode_value);
        for (uint8_t k = 0; k < 32; k++) {
          bool relay_in_use = static_cast<bool>(this->linked_outputs_[i].linked_relays_[k / 8] & (1 << (k % 8)));
          if (relay_in_use == true) {
            ESP_LOGV(TAG, "Output %s has relay %s", this->available_outputs_[i].output_id.c_str(), this->available_relays_[k].relay_id.c_str());
          }
        }
      }
      else {
        ESP_LOGV(TAG, "Output %s available", this->available_outputs_[i].output_id.c_str());
      }
    }
  }
}

void DynamicLampComponent::add_available_lamp(light::LightState* lamp, std::string lamp_id) {
  for (uint8_t lamp_index = 0; lamp_index < 16; lamp_index++) {
    if (this->available_lamps_[lamp_index].available != true) {
      this->available_lamps_[lamp_index].available = true;
      this->available_lamps_[lamp_index].lamp = lamp;
      this->available_lamps_[lamp_index].lamp_id = lamp_id;
      return;
    }
  }
  ESP_LOGW(TAG, "No more lamps available, max 16 lamps supported!");
  this->status_set_warning();
  return;
}

void DynamicLampComponent::add_available_relay(esphome::gpio::GPIOSwitch* relay, std::string relay_id) {
  for (uint8_t relay_index = 0; relay_index < 32; relay_index++) {
    if (this->available_relays_[relay_index].available != true) {
      this->available_relays_[relay_index].available = true;
      this->available_relays_[relay_index].relay = relay;
      this->available_relays_[relay_index].relay_id = relay_id;
      this->available_relays_[relay_index].relay_index = relay_index;
      this->available_relays_[relay_index].in_use = false;
      return;
    }
  }
  ESP_LOGW(TAG, "No more relays available, max 32 relays supported!");
  this->status_set_warning();
  return;
}

bool DynamicLampComponent::add_lamp(std::string name) {
  if (this->lamp_count_ < 15) {
    if (this->available_lamps_[this->lamp_count_].available != true) {
      ESP_LOGW(TAG, "No more lamps available, add defined lights to config - max 16 lamps supported!");
      this->status_set_warning();
      return false;
    }
    this->active_lamps_[this->lamp_count_].active = true;
    strncpy(reinterpret_cast<char*>(this->active_lamps_[this->lamp_count_].name), name.data(), name.length());
    this->active_lamps_[this->lamp_count_].validation_byte = 'L';
    this->active_lamps_[this->lamp_count_].lamp_index = this->lamp_count_;
    this->active_lamps_[this->lamp_count_].used_outputs[0] = 0;
    this->active_lamps_[this->lamp_count_].used_outputs[1] = 0;
    this->fram_->write((0x0000 + (this->lamp_count_ * 28)), reinterpret_cast<unsigned char *>(&this->active_lamps_[this->lamp_count_]), 28);
    this->lamp_count_++;
    ESP_LOGV(TAG, "Added new lamp %s, total lamps now %" PRIu8 "", name.c_str(), this->lamp_count_);
    return true;
  }
  ESP_LOGW(TAG, "No more lamps available, max 16 lamps supported!");
  this->status_set_warning();
  return false;
}

void DynamicLampComponent::remove_lamp(std::string lamp_name) {
  uint8_t i = this->get_lamp_index_by_name_(lamp_name);
  if (i == 255) {
    return;
  }
  for (uint8_t j = 0; j < 16; j++) {
    bool output_in_use = static_cast<bool>(this->active_lamps_[i].used_outputs[j / 8] & (1 << (j % 8)));
    if (output_in_use == true) {
      this->remove_output_from_lamp(i, &this->linked_outputs_[j]);
      this->linked_outputs_[j].lamp_index = 255;
      this->linked_outputs_[j].in_use = false;
      ESP_LOGV(TAG, "Removed output %s from lamp %s", this->available_outputs_[j].output_id.c_str(), this->active_lamps_[i].name);
    }
  }
  uint16_t memaddress = 0 + (i * 28);
  unsigned char empty_lamp[28];
  for (uint8_t m = 0; m < 28; m++) {
    empty_lamp[m] = 0x00;
  }
  this->fram_->write(memaddress, empty_lamp, 28);
  this->active_lamps_[i].active = false;
  this->lamp_count_--;
  ESP_LOGV(TAG, "Removed lamp %s, total lamps now %" PRIu8 "", this->active_lamps_[i].name, this->lamp_count_);
  return;
}

void DynamicLampComponent::rename_lamp(std::string old_lamp_name, std::string new_lamp_name) {
  uint8_t i = this->get_lamp_index_by_name_(old_lamp_name);
  if (i == 255) {
    ESP_LOGW(TAG, "Lamp %s not found, ignoring call to rename lamp!", old_lamp_name.c_str());
    return;
  }
  strncpy(reinterpret_cast<char*>(this->active_lamps_[i].name), new_lamp_name.data(), new_lamp_name.length());
  for (uint8_t j = new_lamp_name.length(); j < 16; j++) {
    this->active_lamps_[i].name[j] = 0x00;
  }
  this->fram_->write((0x0000 + (i * 28)), reinterpret_cast<unsigned char *>(&this->active_lamps_[i]), 28);
  ESP_LOGV(TAG, "Renamed lamp %s to %s", old_lamp_name.c_str(), new_lamp_name.c_str());
  return;
}

uint8_t DynamicLampComponent::get_lamp_count() {
  return this->lamp_count_;
}

std::array<bool, 16> DynamicLampComponent::get_active_lamps() {
  std::array<bool, 16> bool_array;
  for (uint8_t i = 0; i < 16; i++) {
    bool_array[i] = static_cast<bool>(this->active_lamps_[i].active == true);
  }
  return bool_array;
}

void DynamicLampComponent::add_output_to_lamp(std::string lamp_name, LinkedOutput *output, uint8_t output_mode, float output_mode_value, std::string relay_ids) {
  if (output->in_use == true) {
    ESP_LOGW(TAG, "Output %s is already in use, ignoring!", this->available_outputs_[output->output_index].output_id.c_str());
    this->status_set_warning();
    return;
  }
  uint8_t lamp_index = this->get_lamp_index_by_name_(lamp_name);
  if (lamp_index == 255) {
    ESP_LOGW(TAG, "Ignoring call to add output to lamp!", lamp_name.c_str());
    return;
  }
  output->in_use = true;
  output->mode = output_mode;
  output->mode_value = output_mode_value;
  output->lamp_index = lamp_index;
  output->validation_bytes[0] = 'V';
  output->validation_bytes[1] = 'O';
  uint8_t output_index = output->output_index;
  output->linked_relays_[0] = 0;
  output->linked_relays_[1] = 0;
  output->linked_relays_[2] = 0;
  output->linked_relays_[3] = 0;
  this->add_relays_to_output_by_pointer(output, relay_ids);
  ESP_LOGV(TAG, "Size of linked output %s is %" PRIu8 "", this->available_outputs_[output_index].output_id.c_str(), sizeof(*output));
  this->fram_->write((0x01C0 + (output_index * 44)), reinterpret_cast<unsigned char *>(&this->linked_outputs_[output_index]), 44);
  this->active_lamps_[lamp_index].used_outputs[output_index / 8] |= 1 << (output_index % 8);
  this->fram_->write((0x0000 + (lamp_index * 28)), reinterpret_cast<unsigned char *>(&this->active_lamps_[lamp_index]), 28);
  return;
}

void DynamicLampComponent::remove_output_from_lamp(uint8_t lamp_index, LinkedOutput *output) {
  if (!this->active_lamps_[lamp_index].active == true) {
    ESP_LOGV(TAG, "Lamp %" PRIu8 " is not available/active", lamp_index);
  }
  uint8_t output_index = output->output_index;
  ESP_LOGV(TAG, "Output info is %" PRIu8 ", %" PRIu8 "", output_index, output->lamp_index);
  if (output->lamp_index != lamp_index) {
    ESP_LOGW(TAG, "Output %s is not attached to lamp %s, ignoring call to remove output from lamp!", this->available_outputs_[output_index].output_id.c_str(), this->active_lamps_[lamp_index].name);
    return;
  }
  this->active_lamps_[lamp_index].used_outputs[output_index / 8] &= ~(1 << (output_index % 8));
  ESP_LOGV(TAG, "Using address %" PRIu16 " for lamp %s with index %" PRIu8 " to write bytes %" PRIu8 ", %" PRIu8 "", 0x0000 + (lamp_index * 28),
    this->active_lamps_[lamp_index].name, lamp_index, this->active_lamps_[lamp_index].used_outputs[0], this->active_lamps_[lamp_index].used_outputs[1]);
  this->fram_->write((0x0000 + (lamp_index * 28)), reinterpret_cast<unsigned char *>(&this->active_lamps_[lamp_index]), 28);
  output->in_use = false;
  output->lamp_index = 255;
  output->validation_bytes[0] = 'F';
  output->validation_bytes[1] = 'F';
  this->fram_->write((0x01C0 + (output_index * 44)), reinterpret_cast<unsigned char *>(&this->linked_outputs_[output_index]), 44);
  return;
}

void DynamicLampComponent::remove_output_from_lamp_by_name(std::string lamp_name, LinkedOutput *output) {
  uint8_t lamp_index = this->get_lamp_index_by_name_(lamp_name);
  if (lamp_index == 255) {
    ESP_LOGW(TAG, "Ignoring call to remove output from lamp!", lamp_name.c_str());
    return;
  }
  this->remove_output_from_lamp(lamp_index, output);
  return;
}

void DynamicLampComponent::attach_output_to_lamp(std::string lamp_name, std::string output_id, uint8_t mode, float mode_value, std::string relay_ids) {
  uint8_t output_index = 0;
  std::string compare_string = "None (do not attach)";
  if (!lamp_name.compare(compare_string)) {
    while (this->available_outputs_[output_index].available == true) {
      if (this->available_outputs_[output_index].output_id == output_id) {
        if (this->linked_outputs_[output_index].in_use == true) {
          uint8_t current_lamp_index = this->linked_outputs_[output_index].lamp_index;
          this->remove_output_from_lamp(current_lamp_index, &this->linked_outputs_[output_index]);
          ESP_LOGV(TAG, "Removed output %s from lamp %s", this->available_outputs_[output_index].output_id.c_str(), this->active_lamps_[current_lamp_index].name);
        }
      }
      output_index++;
    }
    return;
  } else {
    uint8_t lamp_index = this->get_lamp_index_by_name_(lamp_name);
    if (lamp_index == 255) {
      ESP_LOGW(TAG, "Ignoring call to attach output to lamp!", lamp_name.c_str());
      return;
    }
    while (this->available_outputs_[output_index].available == true) {
      if (!this->available_outputs_[output_index].output_id.compare(output_id)) {
        if (this->linked_outputs_[output_index].in_use == true) {
          uint8_t lamp_index = this->linked_outputs_[output_index].lamp_index;
          this->remove_output_from_lamp(lamp_index, &this->linked_outputs_[output_index]);
          ESP_LOGV(TAG, "Output %s was already in use, removed from lamp %s", this->available_outputs_[output_index].output_id.c_str(), this->active_lamps_[lamp_index].name);
        }
        ESP_LOGV(TAG, "Attaching output %s to lamp %s with mode %" PRIu8 " and mode-value %f", this->available_outputs_[output_index].output_id.c_str(), lamp_name.c_str(), mode, mode_value);
        this->add_output_to_lamp(lamp_name, &this->linked_outputs_[output_index], mode, mode_value, relay_ids);
        ESP_LOGV(TAG, "Attached output %s to lamp %s", this->available_outputs_[output_index].output_id.c_str(), lamp_name.c_str());
        return;
      }
      output_index++;
    }
    ESP_LOGW(TAG, "No output with id %s found, ignoring call to attach output to lamp!", output_id.c_str());
    this->status_set_warning();
    return;
  }
}

std::array<bool, 16> DynamicLampComponent::get_lamp_outputs(uint8_t lamp_number) {
  std::array<bool, 16> bool_array;
  for (uint8_t i = 0; i < 16; i++) {
    bool_array[i] = static_cast<bool>(this->active_lamps_[lamp_number].used_outputs[i / 8] & (1 << (i % 8)));
  }
  return bool_array;
}

uint8_t DynamicLampComponent::get_lamp_index_by_name_(std::string lamp_name) {
  for (uint8_t i = 0; i < this->lamp_count_; i++) {
    if (this->active_lamps_[i].active == true) {
      std::stringstream namestream;
      uint8_t namebyte = 0;
      while (namebyte < 16 && this->active_lamps_[i].name[namebyte] != 0) {
        namestream << this->active_lamps_[i].name[namebyte];
        namebyte++;
      }
      if (!namestream.str().compare(lamp_name)) {
        return i;
      }
    }
  }
  this->status_set_warning();
  ESP_LOGW(TAG, "No lamp with name %s defined !", lamp_name.c_str());
  return 255;
}

std::array<bool, 16> DynamicLampComponent::get_lamp_outputs_by_name_(std::string lamp_name) {
  uint8_t lamp_index = this->get_lamp_index_by_name_(lamp_name);
  if (lamp_index == 255) {
    std::array<bool, 16> bool_array;
    return bool_array;
  }
  return this->get_lamp_outputs(lamp_index);
}

void DynamicLampComponent::add_relays_to_output(std::string output_id, std::string relay_ids) {
  std::vector<uint8_t> relay_id_list = this->split_to_int_vector_(relay_ids);
  std::string relay_id;
  for (uint8_t i = 0; i < relay_id_list.size(); i++) {
    relay_id = this->available_relays_[relay_id_list[i]].relay_id;
    this->add_relay_to_output(output_id, relay_id);
  }
}

void DynamicLampComponent::remove_relays_from_output(std::string output_id, std::string relay_ids) {
  std::vector<uint8_t> relay_id_list = this->split_to_int_vector_(relay_ids);
  std::string relay_id;
  for (uint8_t i = 0; i < relay_id_list.size(); i++) {
    relay_id = this->available_relays_[relay_id_list[i]].relay_id;
    this->remove_relay_from_output(output_id, relay_id);
  }
}

void DynamicLampComponent::add_relay_to_output(std::string output_id, std::string relay_id) {
  uint8_t output_index = 0;
  while (this->available_outputs_[output_index].available == true) {
    if (this->available_outputs_[output_index].output_id == output_id) {
      uint8_t relay_index = 0;
      while (this->available_relays_[relay_index].available == true) {
        if (this->available_relays_[relay_index].relay_id == relay_id) {
          this->linked_outputs_[output_index].linked_relays_[relay_index / 8] |= 1 << (relay_index % 8);
          this->available_relays_[relay_index].in_use = true;
          this->available_relays_[relay_index].output_index = output_index;
          ESP_LOGV(TAG, "Attached relay %s to output %s", this->available_relays_[relay_index].relay_id.c_str(), this->available_outputs_[output_index].output_id.c_str());
          return;
        }
        relay_index++;
      }
    }
    output_index++;
  }
}

void DynamicLampComponent::remove_relay_from_output(std::string output_id, std::string relay_id) {
  uint8_t output_index = 0;
  while (this->available_outputs_[output_index].available == true) {
    if (this->available_outputs_[output_index].output_id == output_id) {
      uint8_t relay_index = 0;
      while (this->available_relays_[relay_index].available == true) {
        if (this->available_relays_[relay_index].relay_id == relay_id) {
          this->available_relays_[relay_index].in_use = false;
          this->available_relays_[relay_index].output_index = 255;
          this->linked_outputs_[output_index].linked_relays_[relay_index / 8] &= ~(1 << (relay_index % 8));
          ESP_LOGV(TAG, "Removed relay %s from output %s", this->available_relays_[relay_index].relay_id.c_str(), this->available_outputs_[output_index].output_id.c_str());
          return;
        }
        relay_index++;
      }
    }
    output_index++;
  }
}

void DynamicLampComponent::remove_relay_from_output_by_index(uint8_t output_index, uint8_t relay_index) {
  this->linked_outputs_[output_index].linked_relays_[relay_index / 8] &= ~(1 << (relay_index % 8));
  this->available_relays_[relay_index].in_use = false;
  this->available_relays_[relay_index].output_index = 255;
  ESP_LOGV(TAG, "Removed relay %s from output %s", this->available_relays_[relay_index].relay_id.c_str(), this->available_outputs_[output_index].output_id.c_str());
}

void DynamicLampComponent::add_relays_to_output_by_pointer(LinkedOutput *output, std::string relay_ids) {
  std::vector<uint8_t> relay_id_list = this->split_to_int_vector_(relay_ids);
  for (uint8_t i = 0; i < relay_id_list.size(); i++) {
    output->linked_relays_[relay_id_list[i] / 8] |= 1 << (relay_id_list[i] % 8);
    this->available_relays_[relay_id_list[i]].in_use = true;
    this->available_relays_[relay_id_list[i]].output_index = output->output_index;
    ESP_LOGV(TAG, "Attached relay %s to output %s", this->available_relays_[relay_id_list[i]].relay_id.c_str(), this->available_outputs_[output->output_index].output_id.c_str());
  }
}

std::string DynamicLampComponent::get_relay_list_for_output(uint8_t output_index) {
  std::string relay_list = "";
  for (uint8_t i = 0; i < 32; i++) {
    bool relay_in_use = static_cast<bool>(this->linked_outputs_[output_index].linked_relays_[i / 8] & (1 << (i % 8)));
    if (relay_in_use == true) {
      if (relay_list.length() > 0) {
        relay_list += ",";
      }
      relay_list += std::to_string(i);
    }
  }
  return relay_list;
}

std::vector<uint8_t> DynamicLampComponent::get_timers_to_execute_(ESPTime& now) {
  uint8_t current_timer_index;
  std::vector<uint8_t> timers_to_execute;
  for (current_timer_index = 0; current_timer_index < 64; current_timer_index++) {
    if (this->timers_[current_timer_index].in_use == true && this->timers_[current_timer_index].active == true) {
      if (this->timers_[current_timer_index].begin_date_year != 0) {
        std::tm valid_from = { 0, 0, 0,
          this->timers_[current_timer_index].begin_date_day,
          this->timers_[current_timer_index].begin_date_month - 1,
          this->timers_[current_timer_index].begin_date_year - 1900
        };
        std::time_t valid_from_timestamp = std::mktime(&valid_from);
        std::tm valid_until = { 0, 0, 0,
          this->timers_[current_timer_index].end_date_day,
          this->timers_[current_timer_index].end_date_month - 1,
          this->timers_[current_timer_index].end_date_year - 1900
        };
        time_t valid_until_timestamp = mktime(&valid_until);
        if (valid_from_timestamp > now.timestamp || valid_until_timestamp < now.timestamp) {
          continue;
        }
      }
      // check if timer is valid for current day of week
      switch (now.day_of_week) {
        case 1:
          if (this->timers_[current_timer_index].sunday != true) {
            continue;
          }
          break;
        case 2:
          if (this->timers_[current_timer_index].monday != true) {
            continue;
          }
          break;
        case 3:
          if (this->timers_[current_timer_index].tuesday != true) {
            continue;
          }
          break;
        case 4:
          if (this->timers_[current_timer_index].wednesday != true) {
            continue;
          }
          break;
        case 5:
          if (this->timers_[current_timer_index].thursday != true) {
            continue;
          }
          break;
        case 6:
          if (this->timers_[current_timer_index].friday != true) {
            continue;
          }
          break;
        case 7:
          if (this->timers_[current_timer_index].saturday != true) {
            continue;
          }
          break;
      }
      uint8_t timer_hour = this->timers_[current_timer_index].hour;
      if (this->timers_[current_timer_index].respect_dst == true) {
        if (now.is_dst != this->timers_[current_timer_index].is_dst) {
          if(this->timers_[current_timer_index].is_dst == false) {
            timer_hour = timer_hour - 1;
          } else {
            timer_hour = timer_hour + 1;
          }
        }
      }
      ESPTime timer_time = now;
      if (timer_hour > 23) {
        timer_hour = timer_hour - 24;
        timer_time.increment_day();
      }
      std::tm timer_time_tm = {
        0,
        this->timers_[current_timer_index].minute,
        this->timers_[current_timer_index].hour,
        timer_time.day_of_month,
        timer_time.month - 1,
        timer_time.year - 1900
      };
      std::time_t timer_timestamp = std::mktime(&timer_time_tm);
      timer_time = ESPTime::from_epoch_local(timer_timestamp);
      if (timer_time == now) {
        timers_to_execute.push_back(current_timer_index);
      }
    }
  }
  return timers_to_execute;
}

time_t DynamicLampComponent::get_next_active_timer_timestamp_(ESPTime& now) {
  // get timers for same day as now() -> "later" that current time
  time_t next_timer_timestamp = this->get_next_timer_timestamp_by_day_(now);
  // do rollover for next day if no timer for today is left
  if (next_timer_timestamp == 0) {
    now.increment_day();
    next_timer_timestamp = this->get_next_timer_timestamp_by_day_(now, true);
  }
  return next_timer_timestamp;
}

time_t DynamicLampComponent::get_next_timer_timestamp_by_day_(ESPTime now, bool ignore_current_time) {
  time_t next_timer_timestamp = 0;
  for (uint8_t current_timer_index = 0; current_timer_index < 64; current_timer_index++) {
    if (this->timers_[current_timer_index].in_use == true && this->timers_[current_timer_index].active == true) {
      // check if timer is valid for current day of week

      if (this->timers_[current_timer_index].begin_date_year != 0) {
        std::tm valid_from = { 0, 0, 0,
          this->timers_[current_timer_index].begin_date_day,
          this->timers_[current_timer_index].begin_date_month - 1,
          this->timers_[current_timer_index].begin_date_year - 1900
        };
        std::time_t valid_from_timestamp = std::mktime(&valid_from);
        std::tm valid_until = { 0, 0, 0,
          this->timers_[current_timer_index].end_date_day,
          this->timers_[current_timer_index].end_date_month - 1,
          this->timers_[current_timer_index].end_date_year - 1900
        };
        time_t valid_until_timestamp = mktime(&valid_until);
        if (valid_from_timestamp > now.timestamp || valid_until_timestamp < now.timestamp) {
          continue;
        }
      }
      switch (now.day_of_week) {
        case 1:
          if (this->timers_[current_timer_index].sunday != true) {
            continue;
          }
          break;
        case 2:
          if (this->timers_[current_timer_index].monday != true) {
            continue;
          }
          break;
        case 3:
          if (this->timers_[current_timer_index].tuesday != true) {
            continue;
          }
          break;
        case 4:
          if (this->timers_[current_timer_index].wednesday != true) {
            continue;
          }
          break;
        case 5:
          if (this->timers_[current_timer_index].thursday != true) {
            continue;
          }
          break;
        case 6:
          if (this->timers_[current_timer_index].friday != true) {
            continue;
          }
          break;
        case 7:
          if (this->timers_[current_timer_index].saturday != true) {
            continue;
          }
          break;
      }
      uint8_t timer_hour = this->timers_[current_timer_index].hour;
      if (this->timers_[current_timer_index].respect_dst == true) {
        if (now.is_dst != this->timers_[current_timer_index].is_dst) {
          if(this->timers_[current_timer_index].is_dst == false) {
            timer_hour = timer_hour - 1;
          } else {
            timer_hour = timer_hour + 1;
          }
        }
      }
      ESPTime timer_time = now;
      if (timer_hour > 23) {
        timer_hour = timer_hour - 24;
        timer_time.increment_day();
      }
      std::tm timer_time_tm = {
        0,
        this->timers_[current_timer_index].minute,
        this->timers_[current_timer_index].hour,
        timer_time.day_of_month,
        timer_time.month - 1,
        timer_time.year - 1900
      };
      time_t timer_timestamp = mktime(&timer_time_tm);
      if (timer_timestamp <= now.timestamp && !ignore_current_time) {
        continue;
      } else {
        if (next_timer_timestamp == 0 || timer_timestamp < next_timer_timestamp) {
          next_timer_timestamp = timer_timestamp;
        }
      }
    }
  }
  return next_timer_timestamp;
}

bool DynamicLampComponent::add_timer(std::string timer_desc, std::string lamp_list_str, bool timer_active, uint8_t action, uint16_t action_value,
                                     uint8_t hour, uint8_t minute, bool monday, bool tuesday, bool wednesday, bool thursday, bool friday,
                                     bool saturday, bool sunday, bool respect_dst, ESPTime valid_from_date, ESPTime valid_until_date) {
  ESPTime now = this->rtc_->now();
  if (now.is_valid() != true) {
    return false;
  }
  if (timer_desc.length() > 32) {
    ESP_LOGW(TAG, "Timer description too long, max 32 characters allowed!");
    this->status_set_warning();
    return false;
  }
  if (lamp_list_str.length() == 0) {
    ESP_LOGW(TAG, "Lamp list empty, ignoring call to add timer!");
    this->status_set_warning();
    return false;
  }
  if (action_value > 100) {
    ESP_LOGW(TAG, "Action value too high, max 100% allowed!");
    this->status_set_warning();
    return false;
  }
  if (action > 2) {
    ESP_LOGW(TAG, "Action value too high, max 2 allowed!");
    this->status_set_warning();
    return false;
  }
  std::vector<bool> lamp_list = this->build_lamp_list_from_list_str_(lamp_list_str);
  DynamicLampTimer new_timer;
  std::stringstream lamp_names_str;
  lamp_names_str.str(std::string());
  strncpy(reinterpret_cast<char *>(new_timer.timer_desc), timer_desc.c_str(), 32);
  unsigned char lamp_list_bytes[2] = {0, 0};
  for (uint8_t i = 0; i < lamp_list.size(); i++) {
    if (lamp_list[i] == true && this->active_lamps_[i].active != true) {
      ESP_LOGW(TAG, "Ignoring lamp number %" PRIu8 " as there is no active lamp with that index!", i);
      continue;
    }
    if (lamp_list[i] == true) {
      lamp_list_bytes[i / 8] |= 1 << (i % 8);
      if (lamp_names_str.str().length() > 0) {
        lamp_names_str << ',' << ' ';
      }
      uint8_t k = 0;
      while (k < 16 && this->active_lamps_[i].name[k] != 0) {
        lamp_names_str << this->active_lamps_[i].name[k];
        k++;
      }
    }
  }
  memcpy(&new_timer.lamp_list, &lamp_list_bytes, 2);
  new_timer.in_use = true;
  new_timer.validation_byte = 'V';
  new_timer.active = timer_active;
  new_timer.action = action;
  new_timer.action_value = action_value;
  new_timer.hour = hour;
  new_timer.minute = minute;
  new_timer.monday = monday;
  new_timer.tuesday = tuesday;
  new_timer.wednesday = wednesday;
  new_timer.thursday = thursday;
  new_timer.friday = friday;
  new_timer.saturday = saturday;
  new_timer.sunday = sunday;
  new_timer.is_dst = now.is_dst;
  new_timer.respect_dst = respect_dst;
  new_timer.begin_date_year = valid_from_date.year;
  new_timer.begin_date_month = valid_from_date.month;
  new_timer.begin_date_day = valid_from_date.day_of_month;
  new_timer.end_date_year = valid_until_date.year;
  new_timer.end_date_month = valid_until_date.month;
  new_timer.end_date_day = valid_until_date.day_of_month;
  unsigned char* timer_as_bytes = static_cast<unsigned char*>(static_cast<void*>(&new_timer));
  ESP_LOGV(TAG, "Added new timer %s, active %d, action %d", new_timer.timer_desc, new_timer.active, new_timer.action);
  ESP_LOGV(TAG, "Timersettings: hour %d, minute %d, monday %d, tuesday %d, wednesday %d, thursday %d, friday %d, saturday %d, sunday %d",
           new_timer.hour, new_timer.minute, new_timer.monday, new_timer.tuesday, new_timer.wednesday,
           new_timer.thursday, new_timer.friday, new_timer.saturday, new_timer.sunday);
  ESP_LOGV(TAG, "Timer active for lamps %s", lamp_names_str.str().c_str());
  ESP_LOGV(TAG, "Timer is valid from: %" PRIu16 "-%" PRIu8 "-%" PRIu8 " until: %" PRIu16 "-%" PRIu8 "-%" PRIu8 "",
           valid_from_date.year, valid_from_date.month, valid_from_date.day_of_month,
           valid_until_date.year, valid_until_date.month, valid_until_date.day_of_month);
  uint8_t save_slot;
  for (save_slot = 0; save_slot < 64; save_slot++) {
    if (this->timers_[save_slot].in_use != true) {
      break;
    }
  }
  if (save_slot == 64) {
    ESP_LOGW(TAG, "No more timer slots available, max 64 timers supported!");
    this->status_set_warning();
    return false;
  }
  this->timers_[save_slot] = new_timer;
  this->fram_->write((0x4000 + (save_slot * 48)), timer_as_bytes, 48);
  time_t next_active_timer_time = this->get_next_active_timer_timestamp_(now);
  ESPTime next_timer_time = ESPTime::from_epoch_local(next_active_timer_time);
  std::string datetime_format = "%d.%m.%Y %H:%M";
  std::string timestring = next_timer_time.strftime(datetime_format.c_str());
  ESP_LOGI(TAG, "setting next execution time %s", timestring.c_str());
  this->next_timer_exec_time_ = next_active_timer_time;
  return true;
}

bool DynamicLampComponent::save_timer(std::string old_timer_desc, std::string new_timer_desc, std::string lamp_list_str, bool timer_active,
                                      uint8_t action, uint16_t action_value, uint8_t hour, uint8_t minute, bool monday, bool tuesday,
                                      bool wednesday, bool thursday, bool friday, bool saturday, bool sunday, bool respect_dst,
                                      ESPTime valid_from_date, ESPTime valid_until_date) {
  ESPTime now = this->rtc_->now();
  if (now.is_valid() != true) {
    return false;
  }
  if (old_timer_desc.length() > 32) {
    ESP_LOGW(TAG, "Timer description too long, max 32 characters allowed!");
    this->status_set_warning();
    return false;
  }
  if (new_timer_desc.length() > 32) {
    ESP_LOGW(TAG, "Timer description too long, max 32 characters allowed!");
    this->status_set_warning();
    return false;
  }
  std::vector<bool> lamp_list = this->build_lamp_list_from_list_str_(lamp_list_str);
  if (!new_timer_desc.compare("")) {
    new_timer_desc = old_timer_desc;
  }
  DynamicLampTimer new_timer;
  std::stringstream lamp_names_str;
  lamp_names_str.str(std::string());
  strncpy(reinterpret_cast<char *>(new_timer.timer_desc), new_timer_desc.c_str(), 32);
  unsigned char lamp_list_bytes[2] = {0, 0};
  for (uint8_t i = 0; i < lamp_list.size(); i++) {
    if (lamp_list[i] == true && this->active_lamps_[i].active != true) {
      ESP_LOGW(TAG, "Ignoring lamp number %" PRIu8 " as there is no active lamp with that index!", i);
      continue;
    }
    if (lamp_list[i] == true) {
      lamp_list_bytes[i / 8] |= 1 << (i % 8);
      if (lamp_names_str.str().length() > 0) {
        lamp_names_str << ',' << ' ';
      }
      uint8_t k = 0;
      while (k < 16 && this->active_lamps_[i].name[k] != 0) {
        lamp_names_str << this->active_lamps_[i].name[k];
        k++;
      }
    }
  }
  memcpy(&new_timer.lamp_list, &lamp_list_bytes, 2);
  new_timer.in_use = true;
  new_timer.validation_byte = 'V';
  new_timer.active = timer_active;
  new_timer.action = action;
  new_timer.action_value = action_value;
  new_timer.hour = hour;
  new_timer.minute = minute;
  new_timer.monday = monday;
  new_timer.tuesday = tuesday;
  new_timer.wednesday = wednesday;
  new_timer.thursday = thursday;
  new_timer.friday = friday;
  new_timer.saturday = saturday;
  new_timer.sunday = sunday;
  new_timer.is_dst = now.is_dst;
  new_timer.respect_dst = respect_dst;
  new_timer.begin_date_year = valid_from_date.year;
  new_timer.begin_date_month = valid_from_date.month;
  new_timer.begin_date_day = valid_from_date.day_of_month;
  new_timer.end_date_year = valid_until_date.year;
  new_timer.end_date_month = valid_until_date.month;
  new_timer.end_date_day = valid_until_date.day_of_month;
  unsigned char* timer_as_bytes = static_cast<unsigned char*>(static_cast<void*>(&new_timer));
  uint8_t save_slot = this->get_timer_index_by_name_(old_timer_desc);
  if (save_slot == 255) {
    this->status_set_warning();
    ESP_LOGW(TAG, "Timer with name %s could not be found, aborting", old_timer_desc.c_str());
  }
  this->timers_[save_slot] = new_timer;
  this->fram_->write((0x4000 + (save_slot * 48)), timer_as_bytes, 48);
  ESP_LOGV(TAG, "Saved timer %s, active %" PRIu8 ", action %" PRIu8 ", action-value: %" PRIu16 "", new_timer.timer_desc, new_timer.active, new_timer.action, new_timer.action_value);
  ESP_LOGV(TAG, "Timersettings: hour %" PRIu8 ", minute %" PRIu8 ", monday %" PRIu8 ", tuesday %" PRIu8 ", wednesday %" PRIu8 ", thursday %" PRIu8 ", friday %" PRIu8 ", saturday %" PRIu8 ", sunday %" PRIu8 "",
           new_timer.hour, new_timer.minute, new_timer.monday, new_timer.tuesday, new_timer.wednesday,
           new_timer.thursday, new_timer.friday, new_timer.saturday, new_timer.sunday);
  ESP_LOGV(TAG, "Timer active for lamps %s", lamp_names_str.str().c_str());
  ESP_LOGV(TAG, "Timer is valid from: %" PRIu16 "-%" PRIu8 "-%" PRIu8 " until: %" PRIu16 "-%" PRIu8 "-%" PRIu8 "",
           valid_from_date.year, valid_from_date.month, valid_from_date.day_of_month,
           valid_until_date.year, valid_until_date.month, valid_until_date.day_of_month);
  time_t next_active_timer_time = this->get_next_active_timer_timestamp_(now);
  ESPTime next_timer_time = ESPTime::from_epoch_local(next_active_timer_time);
  std::string datetime_format = "%d.%m.%Y %H:%M";
  std::string timestring = next_timer_time.strftime(datetime_format.c_str());
  ESP_LOGI(TAG, "setting next execution time %s", timestring.c_str());
  this->next_timer_exec_time_ = next_active_timer_time;
  return true;
}

bool DynamicLampComponent::remove_timer(std::string timer_desc) {
  std::stringstream current_timer_desc;
  for (uint8_t i = 0; i < 64; i++) {
    if (this->timers_[i].in_use == true) {
      current_timer_desc.str(std::string());
      for (uint8_t j = 0; j < 32; j++) {
        if(this->timers_[i].timer_desc[j] != 0) {
          current_timer_desc << this->timers_[i].timer_desc[j];
        } else {
          break;
        }
      }
      if (!timer_desc.compare(current_timer_desc.str())) {
        this->timers_[i].in_use = false;
        unsigned char empty_timer[48];
        for (uint8_t j = 0; j < 48; j++) {
          empty_timer[j] = 0xff;
        }
        this->fram_->write((0x4000 + (i * 48)), empty_timer, 48);
        ESP_LOGV(TAG, "Removed timer %s", timer_desc.c_str());
        return true;
      }
    }
  }
  ESP_LOGW(TAG, "No timer with name %s defined !", timer_desc.c_str());
  return false;
}

std::array<bool, 64> DynamicLampComponent::get_in_use_timers() {
  std::array<bool, 64> bool_array;
  for (uint8_t i = 0; i < 64; i++) {
    bool_array[i] = static_cast<bool>(this->timers_[i].in_use == true);
  }
  return bool_array;
}

std::string DynamicLampComponent::get_timer_desc(uint8_t timer_index) {
  std::stringstream namestream;
  uint8_t i = 0;
  while (i < 32 && this->timers_[timer_index].timer_desc[i] != 0) {
    namestream << this->timers_[timer_index].timer_desc[i];
    i++;
  }
  return namestream.str();
}

uint8_t DynamicLampComponent::get_timer_index_by_name_(std::string timer_desc) {
  uint8_t i = 0;
  uint8_t j;
  std::stringstream current_timer_desc;
  while (i < 64) {
    current_timer_desc.str(std::string());
    for (j = 0; j < 32; j++) {
      if (this->timers_[i].timer_desc[j] == 0) {
        break;
      } else {
        current_timer_desc << this->timers_[i].timer_desc[j];
      }
      if (!timer_desc.compare(current_timer_desc.str())) {
        return i;
      }
    }
    i++;
  }
  ESP_LOGW(TAG, "No timer with description %s found !", timer_desc.c_str());
  this->status_set_warning();
  return 255;
}

DynamicLampTimer DynamicLampComponent::get_timer(uint8_t timer_index) {
  return this->timers_[timer_index];
}

DynamicLampTimer DynamicLampComponent::get_timer_by_name(std::string timer_desc) {
  return this->timers_[this->get_timer_index_by_name_(timer_desc)];
}

std::vector<bool> DynamicLampComponent::build_lamp_list_from_list_str_(std::string lamp_list_str) {
  std::vector<uint8_t> lamp_list_vector = this->split_to_int_vector_(lamp_list_str);
  std::vector<bool> lamp_list;
  while (lamp_list.size() < 16) {
    lamp_list.push_back(false);
  }
  if (lamp_list_vector.size() > 16) {
    ESP_LOGW(TAG, "Too many lamps in list, only 16 supported!");
    this->status_set_warning();
    return lamp_list;
  }
  for (uint8_t i = 0; i < lamp_list_vector.size(); i++) {
    uint8_t lamp_index = lamp_list_vector[i];
    if (lamp_index > 15) {
      ESP_LOGW(TAG, "Lamp index %" PRIu8 " is out of range, only [0-15] supported!", lamp_list_vector[i]);
      this->status_set_warning();
      return lamp_list;
    }
    lamp_list[lamp_index] = true;
  }
  return lamp_list;
}

void DynamicLampComponent::read_fram_settings_to_log() {
  CombinedLamp lamp;
  for (uint8_t i = 0; i < 16; i++) {
    this->fram_->read((0x0000 + (i * 28)), reinterpret_cast<unsigned char *>(&lamp), 28);
    if (lamp.validation_byte == 'L' && lamp.active == true) {
      std::string output_str = "";
      for (uint8_t j = 0; j < 16; j++) {
        bool output_in_use = static_cast<bool>(lamp.used_outputs[j / 8] & (1 << (j % 8)));
        if (output_in_use == true) {
          if (output_str.length() > 0) {
            output_str += ", ";
          }
          std::string str(this->available_outputs_[j].output_id);
          output_str += str;
        }
      }
      ESP_LOGV(TAG, "Lamp %s found: [ active: %d, outputs: %s ]", lamp.name, lamp.active, output_str.c_str());
    }
  }
}

void DynamicLampComponent::read_initialized_settings_to_log() {
  CombinedLamp lamp;
  for (uint8_t i = 0; i < 16; i++) {
    if (this->active_lamps_[i].active == true) {
      std::string output_str = "";
      for (uint8_t j = 0; j < 16; j++) {
        bool output_in_use = static_cast<bool>(this->active_lamps_[i].used_outputs[j / 8] & (1 << (j % 8)));
        if (output_in_use == true) {
          if (output_str.length() > 0) {
            output_str += ", ";
          }
          std::string str(this->available_outputs_[j].output_id);
          output_str += str;
        }
      }
      ESP_LOGV(TAG, "Lamp %s found: [ active: %d, outputs: %s ]", this->active_lamps_[i].name, this->active_lamps_[i].active, output_str.c_str());
    }
  }
}

void DynamicLampComponent::read_fram_timers_to_log() {
  DynamicLampTimer timer = DynamicLampTimer();
  std::stringstream lamp_names_str;
  for (uint8_t i = 0; i < 64; i++) {
    lamp_names_str.str(std::string());
    this->fram_->read((0x4000 + (i * 48)), reinterpret_cast<unsigned char *>(&timer), 48);
    if (timer.validation_byte == 'V' && timer.in_use == true) {
      bool lamp_included;
      uint8_t j;
      for (j = 0; j < 16; j++) {
        uint8_t current_lamp_state = timer.lamp_list[j / 8] & (1 << (j % 8));
        lamp_included = static_cast<bool>(current_lamp_state);
        if (lamp_included == true && this->active_lamps_[j].active == true) {
          if (lamp_names_str.str().length() > 0) {
            lamp_names_str << ',' << ' ';
          }
          uint8_t k = 0;
          while (k < 16 && this->active_lamps_[j].name[k] != 0) {
            lamp_names_str << this->active_lamps_[j].name[k];
            k++;
          }
        }
      }
      ESP_LOGV(TAG, "Timer %s found: [ active: %" PRIu8 ", action: %" PRIu8 ", action-value: %" PRIu16 " hour: %" PRIu8 ", minute: %" PRIu8 ", monday: %" PRIu8 ", tuesday: %" PRIu8 ", wednesday: %" PRIu8 ", thursday: %" PRIu8 ", friday: %" PRIu8 ", saturday: %" PRIu8 ", sunday: %" PRIu8 " ]",
        timer.timer_desc, timer.active, timer.action, timer.action_value, timer.hour, timer.minute, timer.monday,
        timer.tuesday, timer.wednesday, timer.thursday, timer.friday, timer.saturday, timer.sunday);
      ESP_LOGV(TAG, "Timer active for lamps %s", lamp_names_str.str().c_str());
      ESP_LOGV(TAG, "Timer is valid from: %" PRIu16 "-%" PRIu8 "-%" PRIu8 " until: %" PRIu16 "-%" PRIu8 "-%" PRIu8 "",
        timer.begin_date_year, timer.begin_date_month, timer.begin_date_day,
        timer.end_date_year, timer.end_date_month, timer.end_date_day);
    }
  }
}

void DynamicLampComponent::read_initialized_timers_to_log() {
  DynamicLampTimer timer;
  std::stringstream lamp_names_str;
  for (uint8_t i = 0; i < 64; i++) {
    if (this->timers_[i].in_use == true) {
      lamp_names_str.str(std::string());
      timer = this->timers_[i];
      for (uint8_t j = 0; j < 16; j++) {
        uint8_t current_lamp_state = timer.lamp_list[j / 8] & (1 << (j % 8));
        if (static_cast<bool>(current_lamp_state) == true && this->active_lamps_[j].active == true) {
          if (lamp_names_str.str().length() > 0) {
            lamp_names_str << ',' << ' ';
          }
          uint8_t k = 0;
          while (k < 16 && this->active_lamps_[j].name[k] != 0) {
            lamp_names_str << this->active_lamps_[j].name[k];
            k++;
          }
        }
      }
      ESP_LOGV(TAG, "Timer %s found: [ active: %" PRIu8 ", action: %" PRIu8 ", action-value: %" PRIu16 " hour: %" PRIu8 ", minute: %" PRIu8 ", monday: %" PRIu8 ", tuesday: %" PRIu8 ", wednesday: %" PRIu8 ", thursday: %" PRIu8 ", friday: %" PRIu8 ", saturday: %" PRIu8 ", sunday: %" PRIu8 " ]",
        timer.timer_desc, timer.active, timer.action, timer.action_value, timer.hour, timer.minute, timer.monday,
        timer.tuesday, timer.wednesday, timer.thursday, timer.friday, timer.saturday, timer.sunday);
      ESP_LOGV(TAG, "Timer active for lamps %s", lamp_names_str.str().c_str());
      ESP_LOGV(TAG, "Timer is valid from: %" PRIu16 "-%" PRIu8 "-%" PRIu8 " until: %" PRIu16 "-%" PRIu8 "-%" PRIu8 "",
        timer.begin_date_year, timer.begin_date_month, timer.begin_date_day,
        timer.end_date_year, timer.end_date_month, timer.end_date_day);
    }
  }
}

bool DynamicLampComponent::write_state_(uint8_t lamp_number, float state) {
  if (this->active_lamps_[lamp_number].active == true) {
    this->active_lamps_[lamp_number].state_ = state;
    this->active_lamps_[lamp_number].update_ = true;
    return true;
  }
  return false;
}

std::string DynamicLampComponent::get_lamp_name(uint8_t lamp_number) {
  std::stringstream namestream;
  uint8_t i = 0;
  while (i < 16 && this->active_lamps_[lamp_number].name[i] != 0) {
    namestream << this->active_lamps_[lamp_number].name[i];
    i++;
  }
  return namestream.str();
}

void DynamicLampComponent::restore_lamp_settings_() {
  CombinedLamp lamp;
  const char* current_lamp_name;
  switch (this->save_mode_) {
    case SAVE_MODE_LOCAL:
      // ToDo - yet to be implemented
      ESP_LOGW(TAG, "Save mode LOCAL not implemented yet, sorry");
      this->status_set_warning();
      break;
    case SAVE_MODE_FRAM:
      for (uint8_t i = 0; i < 16; i++) {
        lamp = CombinedLamp{0xff, i, false, false, false, false, "", 0.0f, {0,0}};
        this->fram_->read((0x0000 + (i * 28)), reinterpret_cast<unsigned char *>(&lamp), 28);
        if (lamp.validation_byte == 'L' && lamp.active == true) {
          for (uint8_t j = 0; j < 16; j++) {
            if (static_cast<bool>(lamp.used_outputs[j / 8] & (1 << (j % 8))) == true) {
              LinkedOutput output = LinkedOutput{{'F','F'}, false, 255, 255, 0.0f, 0, 0.0f, {0, 0, 0, 0}, 0.0f, 1.0f, false};
              this->fram_->read((0x01C0 + (j * 44)), reinterpret_cast<unsigned char *>(&output), 44);
              if (output.validation_bytes[0] == 'V' && output.validation_bytes[1] == 'O' && output.in_use == true) {
                memcpy(&this->linked_outputs_[j], &output, 44);
                this->linked_outputs_[j].in_use = true;
                this->linked_outputs_[j].lamp_index = i;
                for (uint8_t k = 0; k < 16; k++) {
                  if (static_cast<bool>(this->linked_outputs_[j].linked_relays_[k / 8] & (1 << (k % 8))) == true) {
                    this->available_relays_[k].in_use = true;
                    this->available_relays_[k].output_index = j;
                  }
                }
              } else {
                ESP_LOGW(TAG, "Output %d is not valid, ignoring & removing from lamp", j);
                lamp.used_outputs[j / 8] &= ~(1 << (j % 8));
                this->fram_->write((0x0000 + (i * 28)), reinterpret_cast<unsigned char *>(&lamp), 28);
                continue;
              }
            }
          }
          this->available_lamps_[i].lamp->set_internal(false);
          memcpy(&this->active_lamps_[i], &lamp, 28);
          this->lamp_count_++;
        }
      }
      break;
  }
}

void DynamicLampComponent::restore_timers_() {
  DynamicLampTimer timer;
  switch (this->save_mode_) {
    case SupportedSaveModes::SAVE_MODE_NONE:
      for (uint8_t i = 0; i < 64; i++) {
        this->timers_[i] = DynamicLampTimer();
      }
      break;
    case SupportedSaveModes::SAVE_MODE_LOCAL:
      // ToDo - yet to be implemented
      ESP_LOGW(TAG, "Save mode LOCAL not implemented yet, sorry");
      this->status_set_warning();
      break;
    case SupportedSaveModes::SAVE_MODE_FRAM:
      std::string lamp_names_str;
      for (uint8_t i = 0; i < 64; i++) {
        this->timers_[i] = DynamicLampTimer();
        this->fram_->read((0x4000 + (i * 48)), reinterpret_cast<unsigned char *>(&timer), 48);
        if (timer.validation_byte == 'V' && timer.in_use == true) {
          memcpy(&this->timers_[i], &timer, 48);
        }
      }
      break;
  }
}

void DynamicLampComponent::clear_fram() {
  this->fram_->clear();
  ESP_LOGV(TAG, "Cleared FRAM");
}

std::vector<uint8_t> DynamicLampComponent::split_to_int_vector_(std::string list_str) {
  std::vector<uint8_t> tokens;
  std::stringstream sstream(list_str);
  std::string segment;
  while(std::getline(sstream, segment, ',')) {
    tokens.push_back(static_cast<uint8_t>(atoi(segment.c_str())));
  }
  return tokens;
}

}  // namespace dynamic_lamp
}  // namespace esphome
