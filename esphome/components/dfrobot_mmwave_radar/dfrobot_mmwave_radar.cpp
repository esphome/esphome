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
    // set up the communication interface for the component
    // check if communication works (if not, call mark_failed()).

    cmdQueue_.enqueue(new PowerCommand());

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
    ESP_LOGD(TAG, "Execute PowerCommand");
    return 1; // Command done
}

uint8_t DetRangeCfgCommand::execute() {
    ESP_LOGD(TAG, "Execute DetRangeCfgCommand");
    return 1; // Command done
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
