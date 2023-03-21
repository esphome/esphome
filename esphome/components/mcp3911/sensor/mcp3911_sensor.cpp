#include "mcp3911_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp3911 {

static const char *const TAG = "mcp3911.sensor";

MCP3911Sensor::MCP3911Sensor(uint8_t channel) : channel_(channel) {}

float MCP3911Sensor::get_setup_priority() const { return setup_priority::DATA; }

void MCP3911Sensor::dump_config() {
  LOG_SENSOR("", "MCP3911 Sensor", this);
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  LOG_UPDATE_INTERVAL(this);
}

float MCP3911Sensor::sample() {
    double val=0;
    /*
    this->parent_->c.status.read_reg_incr  = GROUP;
    this->parent_->reg_write(REG_STATUSCOM, REGISTER, 2);
    if (this->channel_ == 0) {
        this->parent_->reg_read(REG_CHANNEL_0, GROUP);
    } else if (this->channel_ == 1) {
        this->parent_->reg_read(REG_CHANNEL_1, GROUP);
    }

    this->parent_->get_value(&val, this->channel_);
    //ESP_LOGD(TAG, "Channel %d Voltage: %f", this->channel_, val);
    return (float) val;
    */
    if (this->channel_ == 0) {
        this->parent_->get_value(&val, 0);
    } else if (this->channel_ == 1) {
        this->parent_->get_value(&val, 1);
    } else return -9999;
   return (float) val;

}

void MCP3911Sensor::update() {
	this->publish_state(this->sample());
}

}  // namespace adc128s102
}  // namespace esphome
