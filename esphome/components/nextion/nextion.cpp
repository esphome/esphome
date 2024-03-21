#include "nextion.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cinttypes>

namespace esphome {
namespace nextion {

static const char *const TAG = "nextion";

void Nextion::setup() {
  this->is_setup_ = false;
  this->ignore_is_setup_ = true;

  // Wake up the nextion
  this->send_command_("bkcmd=0");
  this->send_command_("sleep=0");

  this->send_command_("bkcmd=0");
  this->send_command_("sleep=0");

  // Reboot it
  this->send_command_("rest");

  this->ignore_is_setup_ = false;
}

bool Nextion::send_command_(const std::string &command) {
  if (!this->ignore_is_setup_ && !this->is_setup()) {
    return false;
  }

  ESP_LOGN(TAG, "send_command %s", command.c_str());

  this->write_str(command.c_str());
  const uint8_t to_send[3] = {0xFF, 0xFF, 0xFF};
  this->write_array(to_send, sizeof(to_send));
  return true;
}

bool Nextion::check_connect_() {
  if (this->get_is_connected_())
    return true;

  if (this->comok_sent_ == 0) {
    this->reset_(false);

    this->ignore_is_setup_ = true;
    this->send_command_("boguscommand=0");  // bogus command. needed sometimes after updating
    if (this->exit_reparse_on_start_) {
      this->send_command_("DRAKJHSUYDGBNCJHGJKSHBDN");
    }
    this->send_command_("connect");

    this->comok_sent_ = millis();
    this->ignore_is_setup_ = false;

    return false;
  }

  if (millis() - this->comok_sent_ <= 500)  // Wait 500 ms
    return false;

  std::string response;

  this->recv_ret_string_(response, 0, false);
  if (!response.empty() && response[0] == 0x1A) {
    // Swallow invalid variable name responses that may be caused by the above commands
    ESP_LOGD(TAG, "0x1A error ignored during setup");
    return false;
  }
  if (response.empty() || response.find("comok") == std::string::npos) {
#ifdef NEXTION_PROTOCOL_LOG
    ESP_LOGN(TAG, "Bad connect request %s", response.c_str());
    for (size_t i = 0; i < response.length(); i++) {
      ESP_LOGN(TAG, "response %s %d %d %c", response.c_str(), i, response[i], response[i]);
    }
#endif

    ESP_LOGW(TAG, "Nextion is not connected! ");
    comok_sent_ = 0;
    return false;
  }

  this->ignore_is_setup_ = true;
  ESP_LOGI(TAG, "Nextion is connected");
  this->is_connected_ = true;

  ESP_LOGN(TAG, "connect request %s", response.c_str());

  size_t start;
  size_t end = 0;
  std::vector<std::string> connect_info;
  while ((start = response.find_first_not_of(',', end)) != std::string::npos) {
    end = response.find(',', start);
    connect_info.push_back(response.substr(start, end - start));
  }

  this->is_detected_ = (connect_info.size() == 7);
  if (this->is_detected_) {
    ESP_LOGN(TAG, "Received connect_info %zu", connect_info.size());

    this->device_model_ = connect_info[2];
    this->firmware_version_ = connect_info[3];
    this->serial_number_ = connect_info[5];
    this->flash_size_ = connect_info[6];
  } else {
    ESP_LOGE(TAG, "Nextion returned bad connect value \"%s\"", response.c_str());
  }

  this->ignore_is_setup_ = false;
  this->dump_config();
  return true;
}

void Nextion::reset_(bool reset_nextion) {
  uint8_t d;

  while (this->available()) {  // Clear receive buffer
    this->read_byte(&d);
  };
  this->nextion_queue_.clear();
  this->waveform_queue_.clear();
}

void Nextion::dump_config() {
  ESP_LOGCONFIG(TAG, "Nextion:");
  ESP_LOGCONFIG(TAG, "  Device Model:     %s", this->device_model_.c_str());
  ESP_LOGCONFIG(TAG, "  Firmware Version: %s", this->firmware_version_.c_str());
  ESP_LOGCONFIG(TAG, "  Serial Number:    %s", this->serial_number_.c_str());
  ESP_LOGCONFIG(TAG, "  Flash Size:       %s", this->flash_size_.c_str());
  ESP_LOGCONFIG(TAG, "  Wake On Touch:    %s", YESNO(this->auto_wake_on_touch_));
  ESP_LOGCONFIG(TAG, "  Exit reparse:     %s", YESNO(this->exit_reparse_on_start_));

  if (this->touch_sleep_timeout_ != 0) {
    ESP_LOGCONFIG(TAG, "  Touch Timeout:    %" PRIu32, this->touch_sleep_timeout_);
  }

  if (this->wake_up_page_ != -1) {
    ESP_LOGCONFIG(TAG, "  Wake Up Page:     %d", this->wake_up_page_);
  }

  if (this->start_up_page_ != -1) {
    ESP_LOGCONFIG(TAG, "  Start Up Page:    %d", this->start_up_page_);
  }
}

float Nextion::get_setup_priority() const { return setup_priority::DATA; }
void Nextion::update() {
  if (!this->is_setup()) {
    return;
  }
  if (this->writer_.has_value()) {
    (*this->writer_)(*this);
  }
}

void Nextion::add_sleep_state_callback(std::function<void()> &&callback) {
  this->sleep_callback_.add(std::move(callback));
}

void Nextion::add_wake_state_callback(std::function<void()> &&callback) {
  this->wake_callback_.add(std::move(callback));
}

void Nextion::add_setup_state_callback(std::function<void()> &&callback) {
  this->setup_callback_.add(std::move(callback));
}

void Nextion::add_new_page_callback(std::function<void(uint8_t)> &&callback) {
  this->page_callback_.add(std::move(callback));
}

void Nextion::add_touch_event_callback(std::function<void(uint8_t, uint8_t, bool)> &&callback) {
  this->touch_callback_.add(std::move(callback));
}

void Nextion::update_all_components() {
  if ((!this->is_setup() && !this->ignore_is_setup_) || this->is_sleeping())
    return;

  for (auto *binarysensortype : this->binarysensortype_) {
    binarysensortype->update_component();
  }
  for (auto *sensortype : this->sensortype_) {
    sensortype->update_component();
  }
  for (auto *switchtype : this->switchtype_) {
    switchtype->update_component();
  }
  for (auto *textsensortype : this->textsensortype_) {
    textsensortype->update_component();
  }
}

bool Nextion::send_command_printf(const char *format, ...) {
  if ((!this->is_setup() && !this->ignore_is_setup_) || this->is_sleeping())
    return false;

  char buffer[256];
  va_list arg;
  va_start(arg, format);
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret <= 0) {
    ESP_LOGW(TAG, "Building command for format '%s' failed!", format);
    return false;
  }

  if (this->send_command_(buffer)) {
    this->add_no_result_to_queue_("send_command_printf");
    return true;
  }
  return false;
}

#ifdef NEXTION_PROTOCOL_LOG
void Nextion::print_queue_members_() {
  ESP_LOGN(TAG, "print_queue_members_ (top 10) size %zu", this->nextion_queue_.size());
  ESP_LOGN(TAG, "*******************************************");
  int count = 0;
  for (auto *i : this->nextion_queue_) {
    if (count++ == 10)
      break;

    if (i == nullptr) {
      ESP_LOGN(TAG, "Nextion queue is null");
    } else {
      ESP_LOGN(TAG, "Nextion queue type: %d:%s , name: %s", i->component->get_queue_type(),
               i->component->get_queue_type_string().c_str(), i->component->get_variable_name().c_str());
    }
  }
  ESP_LOGN(TAG, "*******************************************");
}
#endif

void Nextion::loop() {
  if (!this->check_connect_() || this->is_updating_)
    return;

  if (this->nextion_reports_is_setup_ && !this->sent_setup_commands_) {
    this->ignore_is_setup_ = true;
    this->sent_setup_commands_ = true;
    this->send_command_("bkcmd=3");  // Always, returns 0x00 to 0x23 result of serial command.

    this->set_backlight_brightness(this->brightness_);

    // Check if a startup page has been set and send the command
    if (this->start_up_page_ != -1) {
      this->goto_page(this->start_up_page_);
    }

    this->set_auto_wake_on_touch(this->auto_wake_on_touch_);
    this->set_exit_reparse_on_start(this->exit_reparse_on_start_);

    if (this->touch_sleep_timeout_ != 0) {
      this->set_touch_sleep_timeout(this->touch_sleep_timeout_);
    }

    if (this->wake_up_page_ != -1) {
      this->set_wake_up_page(this->wake_up_page_);
    }

    this->ignore_is_setup_ = false;
  }

  this->process_serial_();            // Receive serial data
  this->process_nextion_commands_();  // Process nextion return commands

  if (!this->nextion_reports_is_setup_) {
    if (this->started_ms_ == 0)
      this->started_ms_ = millis();

    if (this->started_ms_ + this->startup_override_ms_ < millis()) {
      ESP_LOGD(TAG, "Manually set nextion report ready");
      this->nextion_reports_is_setup_ = true;
    }
  }
}

bool Nextion::remove_from_q_(bool report_empty) {
  if (this->nextion_queue_.empty()) {
    if (report_empty) {
      ESP_LOGE(TAG, "Nextion queue is empty!");
    }
    return false;
  }

  NextionQueue *nb = this->nextion_queue_.front();
  NextionComponentBase *component = nb->component;

  ESP_LOGN(TAG, "Removing %s from the queue", component->get_variable_name().c_str());

  if (component->get_queue_type() == NextionQueueType::NO_RESULT) {
    if (component->get_variable_name() == "sleep_wake") {
      this->is_sleeping_ = false;
    }
    delete component;  // NOLINT(cppcoreguidelines-owning-memory)
  }
  delete nb;  // NOLINT(cppcoreguidelines-owning-memory)
  this->nextion_queue_.pop_front();
  return true;
}

void Nextion::process_serial_() {
  uint8_t d;

  while (this->available()) {
    read_byte(&d);
    this->command_data_ += d;
  }
}
// nextion.tech/instruction-set/
void Nextion::process_nextion_commands_() {
  if (this->command_data_.length() == 0) {
    return;
  }

  size_t to_process_length = 0;
  std::string to_process;

  ESP_LOGN(TAG, "this->command_data_ %s length %d", this->command_data_.c_str(), this->command_data_.length());
#ifdef NEXTION_PROTOCOL_LOG
  this->print_queue_members_();
#endif
  while ((to_process_length = this->command_data_.find(COMMAND_DELIMITER)) != std::string::npos) {
    ESP_LOGN(TAG, "print_queue_members_ size %zu", this->nextion_queue_.size());
    while (to_process_length + COMMAND_DELIMITER.length() < this->command_data_.length() &&
           static_cast<uint8_t>(this->command_data_[to_process_length + COMMAND_DELIMITER.length()]) == 0xFF) {
      ++to_process_length;
      ESP_LOGN(TAG, "Add extra 0xFF to process");
    }

    this->nextion_event_ = this->command_data_[0];

    to_process_length -= 1;
    to_process = this->command_data_.substr(1, to_process_length);

    switch (this->nextion_event_) {
      case 0x00:  // instruction sent by user has failed
        ESP_LOGW(TAG, "Nextion reported invalid instruction!");
        this->remove_from_q_();

        break;
      case 0x01:  // instruction sent by user was successful

        ESP_LOGVV(TAG, "instruction sent by user was successful");
        ESP_LOGN(TAG, "this->nextion_queue_.empty() %s", this->nextion_queue_.empty() ? "True" : "False");

        this->remove_from_q_();
        if (!this->is_setup_) {
          if (this->nextion_queue_.empty()) {
            ESP_LOGD(TAG, "Nextion is setup");
            this->is_setup_ = true;
            this->setup_callback_.call();
          }
        }

        break;
      case 0x02:  // invalid Component ID or name was used
        ESP_LOGW(TAG, "Nextion reported component ID or name invalid!");
        this->remove_from_q_();
        break;
      case 0x03:  // invalid Page ID or name was used
        ESP_LOGW(TAG, "Nextion reported page ID invalid!");
        this->remove_from_q_();
        break;
      case 0x04:  // invalid Picture ID was used
        ESP_LOGW(TAG, "Nextion reported picture ID invalid!");
        this->remove_from_q_();
        break;
      case 0x05:  // invalid Font ID was used
        ESP_LOGW(TAG, "Nextion reported font ID invalid!");
        this->remove_from_q_();
        break;
      case 0x06:  // File operation fails
        ESP_LOGW(TAG, "Nextion File operation fail!");
        break;
      case 0x09:  // Instructions with CRC validation fails their CRC check
        ESP_LOGW(TAG, "Nextion Instructions with CRC validation fails their CRC check!");
        break;
      case 0x11:  // invalid Baud rate was used
        ESP_LOGW(TAG, "Nextion reported baud rate invalid!");
        break;
      case 0x12:  // invalid Waveform ID or Channel # was used
        if (this->waveform_queue_.empty()) {
          ESP_LOGW(TAG,
                   "Nextion reported invalid Waveform ID or Channel # was used but no waveform sensor in queue found!");
        } else {
          auto &nb = this->waveform_queue_.front();
          NextionComponentBase *component = nb->component;

          ESP_LOGW(TAG, "Nextion reported invalid Waveform ID %d or Channel # %d was used!",
                   component->get_component_id(), component->get_wave_channel_id());

          ESP_LOGN(TAG, "Removing waveform from queue with component id %d and waveform id %d",
                   component->get_component_id(), component->get_wave_channel_id());

          delete nb;  // NOLINT(cppcoreguidelines-owning-memory)
          this->waveform_queue_.pop_front();
        }
        break;
      case 0x1A:  // variable name invalid
        ESP_LOGW(TAG, "Nextion reported variable name invalid!");
        this->remove_from_q_();
        break;
      case 0x1B:  // variable operation invalid
        ESP_LOGW(TAG, "Nextion reported variable operation invalid!");
        this->remove_from_q_();
        break;
      case 0x1C:  // failed to assign
        ESP_LOGW(TAG, "Nextion reported failed to assign variable!");
        this->remove_from_q_();
        break;
      case 0x1D:  // operate EEPROM failed
        ESP_LOGW(TAG, "Nextion reported operating EEPROM failed!");
        break;
      case 0x1E:  // parameter quantity invalid
        ESP_LOGW(TAG, "Nextion reported parameter quantity invalid!");
        this->remove_from_q_();
        break;
      case 0x1F:  // IO operation failed
        ESP_LOGW(TAG, "Nextion reported component I/O operation invalid!");
        break;
      case 0x20:  // undefined escape characters
        ESP_LOGW(TAG, "Nextion reported undefined escape characters!");
        this->remove_from_q_();
        break;
      case 0x23:  // too long variable name
        ESP_LOGW(TAG, "Nextion reported too long variable name!");
        this->remove_from_q_();
        break;
      case 0x24:  //  Serial Buffer overflow occurs
        ESP_LOGW(TAG, "Nextion reported Serial Buffer overflow!");
        break;
      case 0x65: {  // touch event return data
        if (to_process_length != 3) {
          ESP_LOGW(TAG, "Touch event data is expecting 3, received %zu", to_process_length);
          break;
        }

        uint8_t page_id = to_process[0];
        uint8_t component_id = to_process[1];
        uint8_t touch_event = to_process[2];  // 0 -> release, 1 -> press
        ESP_LOGD(TAG, "Got touch event:");
        ESP_LOGD(TAG, "  page_id:      %u", page_id);
        ESP_LOGD(TAG, "  component_id: %u", component_id);
        ESP_LOGD(TAG, "  event type:   %s", touch_event ? "PRESS" : "RELEASE");
        for (auto *touch : this->touch_) {
          touch->process_touch(page_id, component_id, touch_event != 0);
        }
        this->touch_callback_.call(page_id, component_id, touch_event != 0);
        break;
      }
      case 0x66: {  // Nextion initiated new page event return data.
                    // Also is used for sendme command which we never explicitly initiate
        if (to_process_length != 1) {
          ESP_LOGW(TAG, "New page event data is expecting 1, received %zu", to_process_length);
          break;
        }

        uint8_t page_id = to_process[0];
        ESP_LOGD(TAG, "Got new page: %u", page_id);
        this->page_callback_.call(page_id);
        break;
      }
      case 0x67: {  // Touch Coordinate (awake)
        break;
      }
      case 0x68: {  // touch coordinate data (sleep)

        if (to_process_length != 5) {
          ESP_LOGW(TAG, "Touch coordinate data is expecting 5, received %zu", to_process_length);
          ESP_LOGW(TAG, "%s", to_process.c_str());
          break;
        }

        uint16_t x = (uint16_t(to_process[0]) << 8) | to_process[1];
        uint16_t y = (uint16_t(to_process[2]) << 8) | to_process[3];
        uint8_t touch_event = to_process[4];  // 0 -> release, 1 -> press
        ESP_LOGD(TAG, "Got touch event:");
        ESP_LOGD(TAG, "  x:    %u", x);
        ESP_LOGD(TAG, "  y:    %u", y);
        ESP_LOGD(TAG, "  type: %s", touch_event ? "PRESS" : "RELEASE");
        break;
      }

      //  0x70 0x61 0x62 0x31 0x32 0x33 0xFF 0xFF 0xFF
      //  Returned when using get command for a string.
      //  Each byte is converted to char.
      //  data: ab123
      case 0x70:  // string variable data return
      {
        if (this->nextion_queue_.empty()) {
          ESP_LOGW(TAG, "ERROR: Received string return but the queue is empty");
          break;
        }

        NextionQueue *nb = this->nextion_queue_.front();
        NextionComponentBase *component = nb->component;

        if (component->get_queue_type() != NextionQueueType::TEXT_SENSOR) {
          ESP_LOGE(TAG, "ERROR: Received string return but next in queue \"%s\" is not a text sensor",
                   component->get_variable_name().c_str());
        } else {
          ESP_LOGN(TAG, "Received get_string response: \"%s\" for component id: %s, type: %s", to_process.c_str(),
                   component->get_variable_name().c_str(), component->get_queue_type_string().c_str());
          component->set_state_from_string(to_process, true, false);
        }

        delete nb;  // NOLINT(cppcoreguidelines-owning-memory)
        this->nextion_queue_.pop_front();

        break;
      }
        //  0x71 0x01 0x02 0x03 0x04 0xFF 0xFF 0xFF
        //  Returned when get command to return a number
        //  4 byte 32-bit value in little endian order.
        //  (0x01+0x02*256+0x03*65536+0x04*16777216)
        //  data: 67305985
      case 0x71:  // numeric variable data return
      {
        if (this->nextion_queue_.empty()) {
          ESP_LOGE(TAG, "ERROR: Received numeric return but the queue is empty");
          break;
        }

        if (to_process_length == 0) {
          ESP_LOGE(TAG, "ERROR: Received numeric return but no data!");
          break;
        }

        int dataindex = 0;

        int value = 0;

        for (int i = 0; i < 4; ++i) {
          value += to_process[i] << (8 * i);
          ++dataindex;
        }

        NextionQueue *nb = this->nextion_queue_.front();
        NextionComponentBase *component = nb->component;

        if (component->get_queue_type() != NextionQueueType::SENSOR &&
            component->get_queue_type() != NextionQueueType::BINARY_SENSOR &&
            component->get_queue_type() != NextionQueueType::SWITCH) {
          ESP_LOGE(TAG, "ERROR: Received numeric return but next in queue \"%s\" is not a valid sensor type %d",
                   component->get_variable_name().c_str(), component->get_queue_type());
        } else {
          ESP_LOGN(TAG, "Received numeric return for variable %s, queue type %d:%s, value %d",
                   component->get_variable_name().c_str(), component->get_queue_type(),
                   component->get_queue_type_string().c_str(), value);
          component->set_state_from_int(value, true, false);
        }

        delete nb;  // NOLINT(cppcoreguidelines-owning-memory)
        this->nextion_queue_.pop_front();

        break;
      }

      case 0x86: {  // device automatically enters into sleep mode
        ESP_LOGVV(TAG, "Received Nextion entering sleep automatically");
        this->is_sleeping_ = true;
        this->sleep_callback_.call();
        break;
      }
      case 0x87:  // device automatically wakes up
      {
        ESP_LOGVV(TAG, "Received Nextion leaves sleep automatically");
        this->is_sleeping_ = false;
        this->wake_callback_.call();
        this->all_components_send_state_(false);
        break;
      }
      case 0x88:  // system successful start up
      {
        ESP_LOGD(TAG, "system successful start up %zu", to_process_length);
        this->nextion_reports_is_setup_ = true;
        break;
      }
      case 0x89: {  // start SD card upgrade
        break;
      }
      // Data from nextion is
      // 0x90 - Start
      // variable length of 0x70 return formatted data (bytes) that contain the variable name: prints "temp1",0
      // 00 - NULL
      // 00/01 - Single byte for on/off
      // FF FF FF - End
      case 0x90: {  // Switched component
        std::string variable_name;

        // Get variable name
        auto index = to_process.find('\0');
        if (index == std::string::npos || (to_process_length - index - 1) < 1) {
          ESP_LOGE(TAG, "Bad switch component data received for 0x90 event!");
          ESP_LOGN(TAG, "to_process %s %zu %d", to_process.c_str(), to_process_length, index);
          break;
        }

        variable_name = to_process.substr(0, index);
        ++index;

        ESP_LOGN(TAG, "Got Switch:");
        ESP_LOGN(TAG, "  variable_name: %s", variable_name.c_str());
        ESP_LOGN(TAG, "  value:         %d", to_process[0] != 0);

        for (auto *switchtype : this->switchtype_) {
          switchtype->process_bool(variable_name, to_process[index] != 0);
        }
        break;
      }
      // Data from nextion is
      // 0x91 - Start
      // variable length of 0x70 return formatted data (bytes) that contain the variable name: prints "temp1",0
      // 00 - NULL
      // variable length of 0x71 return data: prints temp1.val,0
      // FF FF FF - End
      case 0x91: {  // Sensor component
        std::string variable_name;

        auto index = to_process.find('\0');
        if (index == std::string::npos || (to_process_length - index - 1) != 4) {
          ESP_LOGE(TAG, "Bad sensor component data received for 0x91 event!");
          ESP_LOGN(TAG, "to_process %s %zu %d", to_process.c_str(), to_process_length, index);
          break;
        }

        index = to_process.find('\0');
        variable_name = to_process.substr(0, index);
        // // Get variable name
        int value = 0;
        for (int i = 0; i < 4; ++i) {
          value += to_process[i + index + 1] << (8 * i);
        }

        ESP_LOGN(TAG, "Got sensor:");
        ESP_LOGN(TAG, "  variable_name: %s", variable_name.c_str());
        ESP_LOGN(TAG, "  value:         %d", value);

        for (auto *sensor : this->sensortype_) {
          sensor->process_sensor(variable_name, value);
        }
        break;
      }

      // Data from nextion is
      // 0x92 - Start
      // variable length of 0x70 return formatted data (bytes) that contain the variable name: prints "temp1",0
      // 00 - NULL
      // variable length of 0x70 return formatted data (bytes) that contain the text prints temp1.txt,0
      // 00 - NULL
      // FF FF FF - End
      case 0x92: {  // Text Sensor Component
        std::string variable_name;
        std::string text_value;

        // Get variable name
        auto index = to_process.find('\0');
        if (index == std::string::npos || (to_process_length - index - 1) < 1) {
          ESP_LOGE(TAG, "Bad text sensor component data received for 0x92 event!");
          ESP_LOGN(TAG, "to_process %s %zu %d", to_process.c_str(), to_process_length, index);
          break;
        }

        variable_name = to_process.substr(0, index);
        ++index;

        text_value = to_process.substr(index);

        ESP_LOGN(TAG, "Got Text Sensor:");
        ESP_LOGN(TAG, "  variable_name: %s", variable_name.c_str());
        ESP_LOGN(TAG, "  value:         %s", text_value.c_str());

        // NextionTextSensorResponseQueue *nq = new NextionTextSensorResponseQueue;
        // nq->variable_name = variable_name;
        // nq->state = text_value;
        // this->textsensorq_.push_back(nq);
        for (auto *textsensortype : this->textsensortype_) {
          textsensortype->process_text(variable_name, text_value);
        }
        break;
      }
      // Data from nextion is
      // 0x93 - Start
      // variable length of 0x70 return formatted data (bytes) that contain the variable name: prints "temp1",0
      // 00 - NULL
      // 00/01 - Single byte for on/off
      // FF FF FF - End
      case 0x93: {  // Binary Sensor component
        std::string variable_name;

        // Get variable name
        auto index = to_process.find('\0');
        if (index == std::string::npos || (to_process_length - index - 1) < 1) {
          ESP_LOGE(TAG, "Bad binary sensor component data received for 0x92 event!");
          ESP_LOGN(TAG, "to_process %s %zu %d", to_process.c_str(), to_process_length, index);
          break;
        }

        variable_name = to_process.substr(0, index);
        ++index;

        ESP_LOGN(TAG, "Got Binary Sensor:");
        ESP_LOGN(TAG, "  variable_name: %s", variable_name.c_str());
        ESP_LOGN(TAG, "  value:         %d", to_process[index] != 0);

        for (auto *binarysensortype : this->binarysensortype_) {
          binarysensortype->process_bool(&variable_name[0], to_process[index] != 0);
        }
        break;
      }
      case 0xFD: {  // data transparent transmit finished
        ESP_LOGVV(TAG, "Nextion reported data transmit finished!");
        this->check_pending_waveform_();
        break;
      }
      case 0xFE: {  // data transparent transmit ready
        ESP_LOGVV(TAG, "Nextion reported ready for transmit!");
        if (this->waveform_queue_.empty()) {
          ESP_LOGE(TAG, "No waveforms in queue to send data!");
          break;
        }

        auto &nb = this->waveform_queue_.front();
        auto *component = nb->component;
        size_t buffer_to_send = component->get_wave_buffer_size() < 255 ? component->get_wave_buffer_size()
                                                                        : 255;  // ADDT command can only send 255

        this->write_array(component->get_wave_buffer().data(), static_cast<int>(buffer_to_send));

        ESP_LOGN(TAG, "Nextion sending waveform data for component id %d and waveform id %d, size %zu",
                 component->get_component_id(), component->get_wave_channel_id(), buffer_to_send);

        component->clear_wave_buffer(buffer_to_send);
        delete nb;  // NOLINT(cppcoreguidelines-owning-memory)
        this->waveform_queue_.pop_front();
        break;
      }
      default:
        ESP_LOGW(TAG, "Received unknown event from nextion: 0x%02X", this->nextion_event_);
        break;
    }

    // ESP_LOGN(TAG, "nextion_event_ deleting from 0 to %d", to_process_length + COMMAND_DELIMITER.length() + 1);
    this->command_data_.erase(0, to_process_length + COMMAND_DELIMITER.length() + 1);
    // App.feed_wdt(); Remove before master merge
    this->process_serial_();
  }

  uint32_t ms = millis();

  if (!this->nextion_queue_.empty() && this->nextion_queue_.front()->queue_time + this->max_q_age_ms_ < ms) {
    for (size_t i = 0; i < this->nextion_queue_.size(); i++) {
      NextionComponentBase *component = this->nextion_queue_[i]->component;
      if (this->nextion_queue_[i]->queue_time + this->max_q_age_ms_ < ms) {
        if (this->nextion_queue_[i]->queue_time == 0) {
          ESP_LOGD(TAG, "Removing old queue type \"%s\" name \"%s\" queue_time 0",
                   component->get_queue_type_string().c_str(), component->get_variable_name().c_str());
        }

        if (component->get_variable_name() == "sleep_wake") {
          this->is_sleeping_ = false;
        }

        ESP_LOGD(TAG, "Removing old queue type \"%s\" name \"%s\"", component->get_queue_type_string().c_str(),
                 component->get_variable_name().c_str());

        if (component->get_queue_type() == NextionQueueType::NO_RESULT) {
          if (component->get_variable_name() == "sleep_wake") {
            this->is_sleeping_ = false;
          }
          delete component;  // NOLINT(cppcoreguidelines-owning-memory)
        }

        delete this->nextion_queue_[i];  // NOLINT(cppcoreguidelines-owning-memory)

        this->nextion_queue_.erase(this->nextion_queue_.begin() + i);
        i--;

      } else {
        break;
      }
    }
  }
  ESP_LOGN(TAG, "Loop End");
  // App.feed_wdt(); Remove before master merge
  this->process_serial_();
}  // namespace nextion

void Nextion::set_nextion_sensor_state(int queue_type, const std::string &name, float state) {
  this->set_nextion_sensor_state(static_cast<NextionQueueType>(queue_type), name, state);
}

void Nextion::set_nextion_sensor_state(NextionQueueType queue_type, const std::string &name, float state) {
  ESP_LOGN(TAG, "Received state:");
  ESP_LOGN(TAG, "  variable:   %s", name.c_str());
  ESP_LOGN(TAG, "  state:      %lf", state);
  ESP_LOGN(TAG, "  queue type: %d", queue_type);

  switch (queue_type) {
    case NextionQueueType::SENSOR: {
      for (auto *sensor : this->sensortype_) {
        if (name == sensor->get_variable_name()) {
          sensor->set_state(state, true, true);
          break;
        }
      }
      break;
    }
    case NextionQueueType::BINARY_SENSOR: {
      for (auto *sensor : this->binarysensortype_) {
        if (name == sensor->get_variable_name()) {
          sensor->set_state(state != 0, true, true);
          break;
        }
      }
      break;
    }
    case NextionQueueType::SWITCH: {
      for (auto *sensor : this->switchtype_) {
        if (name == sensor->get_variable_name()) {
          sensor->set_state(state != 0, true, true);
          break;
        }
      }
      break;
    }
    default: {
      ESP_LOGW(TAG, "set_nextion_sensor_state does not support a queue type %d", queue_type);
    }
  }
}

void Nextion::set_nextion_text_state(const std::string &name, const std::string &state) {
  ESP_LOGD(TAG, "Received state:");
  ESP_LOGD(TAG, "  variable: %s", name.c_str());
  ESP_LOGD(TAG, "  state:    %s", state.c_str());

  for (auto *sensor : this->textsensortype_) {
    if (name == sensor->get_variable_name()) {
      sensor->set_state(state, true, true);
      break;
    }
  }
}

void Nextion::all_components_send_state_(bool force_update) {
  ESP_LOGD(TAG, "all_components_send_state_ ");
  for (auto *binarysensortype : this->binarysensortype_) {
    if (force_update || binarysensortype->get_needs_to_send_update())
      binarysensortype->send_state_to_nextion();
  }
  for (auto *sensortype : this->sensortype_) {
    if ((force_update || sensortype->get_needs_to_send_update()) && sensortype->get_wave_chan_id() == 0)
      sensortype->send_state_to_nextion();
  }
  for (auto *switchtype : this->switchtype_) {
    if (force_update || switchtype->get_needs_to_send_update())
      switchtype->send_state_to_nextion();
  }
  for (auto *textsensortype : this->textsensortype_) {
    if (force_update || textsensortype->get_needs_to_send_update())
      textsensortype->send_state_to_nextion();
  }
}

void Nextion::update_components_by_prefix(const std::string &prefix) {
  for (auto *binarysensortype : this->binarysensortype_) {
    if (binarysensortype->get_variable_name().find(prefix, 0) != std::string::npos)
      binarysensortype->update_component_settings(true);
  }
  for (auto *sensortype : this->sensortype_) {
    if (sensortype->get_variable_name().find(prefix, 0) != std::string::npos)
      sensortype->update_component_settings(true);
  }
  for (auto *switchtype : this->switchtype_) {
    if (switchtype->get_variable_name().find(prefix, 0) != std::string::npos)
      switchtype->update_component_settings(true);
  }
  for (auto *textsensortype : this->textsensortype_) {
    if (textsensortype->get_variable_name().find(prefix, 0) != std::string::npos)
      textsensortype->update_component_settings(true);
  }
}

uint16_t Nextion::recv_ret_string_(std::string &response, uint32_t timeout, bool recv_flag) {
  uint16_t ret = 0;
  uint8_t c = 0;
  uint8_t nr_of_ff_bytes = 0;
  uint64_t start;
  bool exit_flag = false;
  bool ff_flag = false;

  start = millis();

  while ((timeout == 0 && this->available()) || millis() - start <= timeout) {
    if (!this->available()) {
      App.feed_wdt();
      delay(1);
      continue;
    }

    this->read_byte(&c);
    if (c == 0xFF) {
      nr_of_ff_bytes++;
    } else {
      nr_of_ff_bytes = 0;
      ff_flag = false;
    }

    if (nr_of_ff_bytes >= 3)
      ff_flag = true;

    response += (char) c;
    if (recv_flag) {
      if (response.find(0x05) != std::string::npos) {
        exit_flag = true;
      }
    }
    App.feed_wdt();
    delay(2);

    if (exit_flag || ff_flag) {
      break;
    }
  }

  if (ff_flag)
    response = response.substr(0, response.length() - 3);  // Remove last 3 0xFF

  ret = response.length();
  return ret;
}

/**
 * @brief
 *
 * @param variable_name Name for the queue
 */
void Nextion::add_no_result_to_queue_(const std::string &variable_name) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  nextion::NextionQueue *nextion_queue = new nextion::NextionQueue;

  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  nextion_queue->component = new nextion::NextionComponentBase;
  nextion_queue->component->set_variable_name(variable_name);

  nextion_queue->queue_time = millis();

  this->nextion_queue_.push_back(nextion_queue);

  ESP_LOGN(TAG, "Add to queue type: NORESULT component %s", nextion_queue->component->get_variable_name().c_str());
}

/**
 * @brief
 *
 * @param variable_name Variable name for the queue
 * @param command
 */
void Nextion::add_no_result_to_queue_with_command_(const std::string &variable_name, const std::string &command) {
  if ((!this->is_setup() && !this->ignore_is_setup_) || command.empty())
    return;

  if (this->send_command_(command)) {
    this->add_no_result_to_queue_(variable_name);
  }
}

bool Nextion::add_no_result_to_queue_with_ignore_sleep_printf_(const std::string &variable_name, const char *format,
                                                               ...) {
  if ((!this->is_setup() && !this->ignore_is_setup_))
    return false;

  char buffer[256];
  va_list arg;
  va_start(arg, format);
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret <= 0) {
    ESP_LOGW(TAG, "Building command for format '%s' failed!", format);
    return false;
  }

  this->add_no_result_to_queue_with_command_(variable_name, buffer);
  return true;
}

/**
 * @brief Sends a formatted command to the nextion
 *
 * @param variable_name Variable name for the queue
 * @param format The printf-style command format, like "vis %s,0"
 * @param ... The format arguments
 */
bool Nextion::add_no_result_to_queue_with_printf_(const std::string &variable_name, const char *format, ...) {
  if ((!this->is_setup() && !this->ignore_is_setup_) || this->is_sleeping())
    return false;

  char buffer[256];
  va_list arg;
  va_start(arg, format);
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret <= 0) {
    ESP_LOGW(TAG, "Building command for format '%s' failed!", format);
    return false;
  }

  this->add_no_result_to_queue_with_command_(variable_name, buffer);
  return true;
}

/**
 * @brief
 *
 * @param variable_name Variable name for the queue
 * @param variable_name_to_send Variable name for the left of the command
 * @param state_value Value to set
 * @param is_sleep_safe The command is safe to send when the Nextion is sleeping
 */

void Nextion::add_no_result_to_queue_with_set(NextionComponentBase *component, int state_value) {
  this->add_no_result_to_queue_with_set(component->get_variable_name(), component->get_variable_name_to_send(),
                                        state_value);
}

void Nextion::add_no_result_to_queue_with_set(const std::string &variable_name,
                                              const std::string &variable_name_to_send, int state_value) {
  this->add_no_result_to_queue_with_set_internal_(variable_name, variable_name_to_send, state_value);
}

void Nextion::add_no_result_to_queue_with_set_internal_(const std::string &variable_name,
                                                        const std::string &variable_name_to_send, int state_value,
                                                        bool is_sleep_safe) {
  if ((!this->is_setup() && !this->ignore_is_setup_) || (!is_sleep_safe && this->is_sleeping()))
    return;

  this->add_no_result_to_queue_with_ignore_sleep_printf_(variable_name, "%s=%d", variable_name_to_send.c_str(),
                                                         state_value);
}

/**
 * @brief
 *
 * @param variable_name Variable name for the queue
 * @param variable_name_to_send Variable name for the left of the command
 * @param state_value String value to set
 * @param is_sleep_safe The command is safe to send when the Nextion is sleeping
 */
void Nextion::add_no_result_to_queue_with_set(NextionComponentBase *component, const std::string &state_value) {
  this->add_no_result_to_queue_with_set(component->get_variable_name(), component->get_variable_name_to_send(),
                                        state_value);
}
void Nextion::add_no_result_to_queue_with_set(const std::string &variable_name,
                                              const std::string &variable_name_to_send,
                                              const std::string &state_value) {
  this->add_no_result_to_queue_with_set_internal_(variable_name, variable_name_to_send, state_value);
}

void Nextion::add_no_result_to_queue_with_set_internal_(const std::string &variable_name,
                                                        const std::string &variable_name_to_send,
                                                        const std::string &state_value, bool is_sleep_safe) {
  if ((!this->is_setup() && !this->ignore_is_setup_) || (!is_sleep_safe && this->is_sleeping()))
    return;

  this->add_no_result_to_queue_with_printf_(variable_name, "%s=\"%s\"", variable_name_to_send.c_str(),
                                            state_value.c_str());
}

void Nextion::add_to_get_queue(NextionComponentBase *component) {
  if ((!this->is_setup() && !this->ignore_is_setup_))
    return;

  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  nextion::NextionQueue *nextion_queue = new nextion::NextionQueue;

  nextion_queue->component = component;
  nextion_queue->queue_time = millis();

  ESP_LOGN(TAG, "Add to queue type: %s component %s", component->get_queue_type_string().c_str(),
           component->get_variable_name().c_str());

  std::string command = "get " + component->get_variable_name_to_send();

  if (this->send_command_(command)) {
    this->nextion_queue_.push_back(nextion_queue);
  }
}

/**
 * @brief Add addt command to the queue
 *
 * @param component_id The waveform component id
 * @param wave_chan_id The waveform channel to send it to
 * @param buffer_to_send The buffer size
 * @param buffer_size The buffer data
 */
void Nextion::add_addt_command_to_queue(NextionComponentBase *component) {
  if ((!this->is_setup() && !this->ignore_is_setup_) || this->is_sleeping())
    return;

  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  nextion::NextionQueue *nextion_queue = new nextion::NextionQueue;

  nextion_queue->component = component;
  nextion_queue->queue_time = millis();

  this->waveform_queue_.push_back(nextion_queue);
  if (this->waveform_queue_.size() == 1)
    this->check_pending_waveform_();
}

void Nextion::check_pending_waveform_() {
  if (this->waveform_queue_.empty())
    return;

  auto *nb = this->waveform_queue_.front();
  auto *component = nb->component;
  size_t buffer_to_send = component->get_wave_buffer_size() < 255 ? component->get_wave_buffer_size()
                                                                  : 255;  // ADDT command can only send 255

  std::string command = "addt " + to_string(component->get_component_id()) + "," +
                        to_string(component->get_wave_channel_id()) + "," + to_string(buffer_to_send);
  if (!this->send_command_(command)) {
    delete nb;  // NOLINT(cppcoreguidelines-owning-memory)
    this->waveform_queue_.pop_front();
  }
}

void Nextion::set_writer(const nextion_writer_t &writer) { this->writer_ = writer; }

ESPDEPRECATED("set_wait_for_ack(bool) is deprecated and has no effect", "v1.20")
void Nextion::set_wait_for_ack(bool wait_for_ack) { ESP_LOGE(TAG, "This command is deprecated"); }

}  // namespace nextion
}  // namespace esphome
