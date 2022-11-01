#include "dfrobot_mmwave_radar.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfrobot_mmwave_radar {

static const char *const TAG = "dfrobot_mmwave_radar";
const char ASCII_CR = 0x0D;
const char ASCII_LF = 0x0A;

void DfrobotMmwaveRadarComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "Range minimum: %f m", range_min_);
    ESP_LOGCONFIG(TAG, "Range maximum: %f m", range_max_);
    ESP_LOGCONFIG(TAG, "Delay after detect: %f s", delay_after_detect_);
    ESP_LOGCONFIG(TAG, "Delay after disappear: %f s", delay_after_disappear_);
}

void DfrobotMmwaveRadarComponent::setup() {
    cmdQueue_.enqueue(new PowerCommand(this, 1));

    // Put command in queue which reads sensor state.
    // Command automatically puts itself in queue again after executed.
    cmdQueue_.enqueue(new ReadStateCommand(this));
}

void DfrobotMmwaveRadarComponent::loop() {
    Command * cmd = cmdQueue_.peek();
    if(cmd == nullptr)
        return;

    // execute of the same command might be called multiple times
    // until it returns 1, which means it is done. Then it is removed
    // from the queue.
    if(cmd->execute()) {
        cmdQueue_.dequeue();
        delete cmd;
    }
}

uint8_t DfrobotMmwaveRadarComponent::read_message() {
    while (this->available()) {
        uint8_t byte;
        this->read_byte(&byte);

        if (this->read_pos_ == MMWAVE_READ_BUFFER_LENGTH)
            this->read_pos_ = 0;

        ESP_LOGVV(TAG, "Buffer pos: %u %d", this->read_pos_, byte);

        if (byte == ASCII_CR)
            continue;
        if (byte >= 0x7F)
            byte = '?';  // needs to be valid utf8 string for log functions.
        this->read_buffer_[this->read_pos_] = byte;

        if (this->read_pos_ == 9 && byte == '>')
            this->read_buffer_[++this->read_pos_] = ASCII_LF;

        if (this->read_buffer_[this->read_pos_] == ASCII_LF) {
            this->read_buffer_[this->read_pos_] = 0;
            this->read_pos_ = 0;
            ESP_LOGV(TAG, "Message: %s", this->read_buffer_);
            return 1; // Full message in buffer
        } else {
            this->read_pos_++;
        }
    }
    return 0; // No full message yet
}

uint8_t DfrobotMmwaveRadarComponent::find_prompt() {
    if(this->read_message()) {
        std::string message(this->read_buffer_);
        if(message.rfind("leapMMW:/>") != std::string::npos) {
            return 1; // Prompt found
        }
    }
    return 0; // Not found yet
}

uint8_t DfrobotMmwaveRadarComponent::send_cmd(const char * cmd) {
    // The interval between two commands must be larger than 1000ms.
    if(millis() - ts_last_cmd_sent_ > 1000) {
        this->write_str(cmd);
        ts_last_cmd_sent_ = millis();
        return 1; // Command sent
    }
    // Could not send command yet as last command was sent less than 1001ms ago.
    return 0;
}

int8_t CircularCommandQueue::enqueue(Command * cmd) {
    if(this->isFull()) {
        ESP_LOGE(TAG, "Command queue is full");
        return -1;
    }
    else if(this->isEmpty())
        front_++;
    rear_ = (rear_ + 1) % COMMAND_QUEUE_SIZE;
    commands_[rear_] = cmd;
    return 1;
}

Command * CircularCommandQueue::dequeue() {
    if(this->isEmpty())
        return nullptr;
    Command * temp = commands_[front_];
    if(front_ == rear_) {
        front_ = -1;
        rear_ = -1;
    }
    else
        front_ = (front_ + 1) % COMMAND_QUEUE_SIZE;

    return temp;
}

Command * CircularCommandQueue::peek() {
    if(this->isEmpty())
        return nullptr;
    return commands_[front_];
}

bool CircularCommandQueue::isEmpty() {
    return front_ == -1;
}

bool CircularCommandQueue::isFull() {
    return (rear_ + 1) % COMMAND_QUEUE_SIZE == front_;
}

uint8_t ReadStateCommand::execute() {
    if(component_->read_message()) {
        int8_t state = parse_sensing_results();
        if(state >= 0) {
            // Automatically self queue again to constantly read state
            component_->cmdQueue_.enqueue(new ReadStateCommand(component_));

            ESP_LOGD(TAG, "Sensor state: %d", state);
            return 1; // Command done
        }
    }
    return 0; // Command not done yet.
}

int8_t ReadStateCommand::parse_sensing_results() {
    std::string message(component_->read_buffer_);
    if(message.rfind("$JYBSS,0, , , *") != std::string::npos) {
        // TODO: set sensor state
        return 0;
    }
    else if(message.rfind("$JYBSS,1, , , *") != std::string::npos) {
        // TODO: set sensor state
        return 1;
    }
    else
        return -1;
}

uint8_t PowerCommand::execute() {
    if(cmd_sent_) {
        // ESP_LOGD(TAG, "Waiting for response");
        if(component_->read_message()) {
            std::string message(component_->read_buffer_);
            if(message.rfind("is not recognized as a CLI command") != std::string::npos) {
                ESP_LOGD(TAG, "Command not recognized properly by sensor");
                if(retries_left_ > 0) {
                    retries_left_ -= 1;
                    cmd_sent_ = false;
                    ESP_LOGD(TAG, "Retrying...");
                }
                else {
                    component_->find_prompt();
                    return 1; // Command done
                }
            }
            if(message.compare("sensor stopped already") == 0) {
                ESP_LOGI(TAG, "Stopped sensor (already stopped)");
                component_->find_prompt();
                return 1; // Command done
            }
            else if(message.compare("sensor started already") == 0) {
                ESP_LOGI(TAG, "Started sensor (already started)");
                component_->find_prompt();
                return 1; // Command done
            }
            else if(message.compare("new parameter isn't save, "\
                                    "can't startSensor") == 0) {
                ESP_LOGE(TAG, "Can't start sensor! (Use SaveCfgCommand to save config first)");
                component_->find_prompt();
                return 1; // Command done
            }
            else if(message.compare("Done") == 0) {
                if(powerOn_)
                    ESP_LOGI(TAG, "Started sensor");
                else
                    ESP_LOGI(TAG, "Stopped sensor");
                component_->find_prompt();
                return 1; // Command done
            }
        }
        if(millis() - component_->ts_last_cmd_sent_ > 500) {
            ESP_LOGD(TAG, "Command timeout");
            if(retries_left_ > 0) {
                retries_left_ -= 1;
                cmd_sent_ = false;
                ESP_LOGD(TAG, "Retrying...");
            }
            else {
                ESP_LOGE(TAG, "PowerCommand error: No response");
                return 1; // Command done
            }
        }
    }
    else if(component_->send_cmd(cmd_.c_str())) {
        if(powerOn_)
            ESP_LOGD(TAG, "Starting sensor");
        else
            ESP_LOGD(TAG, "Stopping sensor");
        cmd_sent_ = true;
    }
    return 0; // Command not done yet
}

DetRangeCfgCommand::DetRangeCfgCommand(DfrobotMmwaveRadarComponent *component,
                                        float min1, float max1,
                                        float min2, float max2,
                                        float min3, float max3,
                                        float min4, float max4
                                      ) {
    char tmp_cmd[46] = {0};

    if(min1 < 0 || max1 < 0) {
        min1_ = min1 = 0;
        max1_ = max1 = 0;
        min2_ = min2 = max2_ = max2 =
        min3_ = min3 = max3_ = max3 =
        min4_ = min4 = max4_ = max4 = -1;

        ESP_LOGW(TAG, "DetRangeCfgCommand invalid input parameters. Using range config 0 0.");

        sprintf(tmp_cmd, "detRangeCfg -1 0 0");
    }
    else if(min2 < 0 || max2 < 0) {
        min1_ = min1 = round(min1 / 0.15) * 0.15;
        max1_ = max1 = round(max1 / 0.15) * 0.15;
        min2_ = min2 = max2_ = max2 =
        min3_ = min3 = max3_ = max3 =
        min4_ = min4 = max4_ = max4 = -1;

        sprintf(tmp_cmd, "detRangeCfg -1 %.0f %.0f",
            min1 / 0.15, max1 / 0.15);
    }
    else if(min3 < 0 || max3 < 0) {
        min1_ = min1 = round(min1 / 0.15) * 0.15;
        max1_ = max1 = round(max1 / 0.15) * 0.15;
        min2_ = min2 = round(min2 / 0.15) * 0.15;
        max2_ = max2 = round(max2 / 0.15) * 0.15;
        min3_ = min3 = max3_ = max3 =
        min4_ = min4 = max4_ = max4 = -1;

        sprintf(tmp_cmd, "detRangeCfg -1 %.0f %.0f %.0f %.0f",
            min1 / 0.15, max1 / 0.15,
            min2 / 0.15, max2 / 0.15);
    }
    else if(min4 < 0 || max4 < 0) {
        min1_ = min1 = round(min1 / 0.15) * 0.15;
        max1_ = max1 = round(max1 / 0.15) * 0.15;
        min2_ = min2 = round(min2 / 0.15) * 0.15;
        max2_ = max2 = round(max2 / 0.15) * 0.15;
        min3_ = min3 = round(min3 / 0.15) * 0.15;
        max3_ = max3 = round(max3 / 0.15) * 0.15;
        min4_ = min4 = max4_ = max4 = -1;

        sprintf(tmp_cmd, "detRangeCfg -1 "\
                         "%.0f %.0f %.0f %.0f %.0f %.0f",
            min1 / 0.15, max1 / 0.15,
            min2 / 0.15, max2 / 0.15,
            min3 / 0.15, max3 / 0.15);
    }
    else {
        min1_ = min1 = round(min1 / 0.15) * 0.15;
        max1_ = max1 = round(max1 / 0.15) * 0.15;
        min2_ = min2 = round(min2 / 0.15) * 0.15;
        max2_ = max2 = round(max2 / 0.15) * 0.15;
        min3_ = min3 = round(min3 / 0.15) * 0.15;
        max3_ = max3 = round(max3 / 0.15) * 0.15;
        min4_ = min4 = round(min4 / 0.15) * 0.15;
        max4_ = max4 = round(max4 / 0.15) * 0.15;

        sprintf(tmp_cmd, "detRangeCfg -1 "\
                         "%.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f",
            min1 / 0.15, max1 / 0.15,
            min2 / 0.15, max2 / 0.15,
            min3 / 0.15, max3 / 0.15,
            min4 / 0.15, max4 / 0.15);
    }

    component_ = component;
    min1_ = min1; max1_ = max1;
    min2_ = min2; max2_ = max2;
    min3_ = min3; max3_ = max3;
    min4_ = min4; max4_ = max4;

    cmd_ = std::string(tmp_cmd);
};

uint8_t DetRangeCfgCommand::execute() {
    if(cmd_sent_) {
        // TODO: Implement DetRangeCfg response parse
        // TODO: Implement retry
        if(component_->read_message()) {
            std::string message(component_->read_buffer_);
            if(message.rfind("is not recognized as a CLI command") != std::string::npos) {
                ESP_LOGD(TAG, "Command not recognized properly by sensor");
                if(retries_left_ > 0) {
                    retries_left_ -= 1;
                    cmd_sent_ = false;
                    ESP_LOGD(TAG, "Retrying...");
                }
                else {
                    component_->find_prompt();
                    return 1; // Command done
                }
            }
            else if(message.compare("sensor is not stopped") == 0) {
                ESP_LOGE(TAG, "Cannot configure range config. Sensor is not stopped!");
                component_->find_prompt();
                return 1; // Command done
            }
            else if(message.compare("Done") == 0) {
                ESP_LOGI(TAG, "Updated detection area config.");
                component_->find_prompt();
                return 1; // Command done
            }
        }
        if(millis() - component_->ts_last_cmd_sent_ > 500) {
            ESP_LOGD(TAG, "Command timeout");
            if(retries_left_ > 0) {
                retries_left_ -= 1;
                cmd_sent_ = false;
                ESP_LOGD(TAG, "Retrying...");
            }
            else {
                ESP_LOGE(TAG, "DetRangeCfgCommand error: No response");
                return 1; // Command done
            }
        }
    }
    else if(component_->send_cmd(cmd_.c_str())) {
        if(min4_ >= 0) {
            ESP_LOGD(TAG, "Setting detection area config to "\
                          "%.2fm-%.2fm, "\
                          "%.2fm-%.2fm, "\
                          "%.2fm-%.2fm, "\
                          "%.2fm-%.2fm",
                          min1_, max1_, min2_, max2_,
                          min3_, max3_, min4_, max4_);
        }
        else if(min3_ >= 0) {
            ESP_LOGD(TAG, "Setting detection area config to "\
                          "%.2fm-%.2fm, "\
                          "%.2fm-%.2fm, "\
                          "%.2fm-%.2fm",
                          min1_, max1_, min2_, max2_, min3_, max3_);
        }
        else if(min2_ >= 0) {
            ESP_LOGD(TAG, "Setting detection area config to "\
                          "%.2fm-%.2fm, %.2fm-%.2fm",
                          min1_, max1_, min2_, max2_);
        }
        else {
            ESP_LOGD(TAG, "Setting detection area config to "\
                          "%.2fm-%.2fm", min1_, max1_);
        }
        cmd_sent_ = true;
    }
    return 0; // Command not done yet
}

uint8_t OutputLatencyCommand::execute() {
    ESP_LOGD(TAG, "Execute OutputLatencyCommand");
    return 1; // Command done
}

uint8_t SensorCfgStartCommand::execute() {
    ESP_LOGD(TAG, "Execute SensorCfgStartCommand");
    return 1; // Command done
}

}  // namespace dfrobot_mmwave_radar
}  // namespace esphome
