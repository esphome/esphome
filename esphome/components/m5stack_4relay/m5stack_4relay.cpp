#include "m5stack_4relay.h"
#include "esphome/core/log.h"
#include <bitset>
namespace esphome
{
  namespace m5stack_4relay
  {
    const uint8_t MODE_CONTROL_REG = 0x10;
    const uint8_t RELAY_CONTROL_REG = 0x11;
    uint8_t component_status;
    static const char *const TAG = "m5stack_4relay.switch";

    void M5STACK4RELAYSwitchComponent::setup()
    {
      ESP_LOGCONFIG(TAG, "Setting up M5STACK4RELAY (0x%02X)...", this->address_);
      component_status = 0;
      get_status(&component_status);
      ESP_LOGD(TAG, "Setup Status 0x%02X", component_status);
      // this->address_;
    }

    M5STACK4RELAYChannel *M5STACK4RELAYSwitchComponent::create_channel(uint8_t channel)
    {
      auto *c = new M5STACK4RELAYChannel(this, channel);
      return c;
    }

    void M5STACK4RELAYSwitchComponent::get_status(uint8_t *state)
    {
      this->read_byte(RELAY_CONTROL_REG);
    }

    bool M5STACK4RELAYSwitchComponent::set_channel_value_(uint8_t channel, bool state)
    {
      get_status(&component_status);

      ESP_LOGD(TAG, "Current Status 0x%02X", component_status);
      ESP_LOGD(TAG, "Desired channel: %1u", channel);
      ESP_LOGD(TAG, "Desired state: %1u", state);

      // M5Stack thought it would be best if the relay mappings are backwards. So, bit 0 is relay 4.
      // We'll just remap it here. The best way to do this would probably be a lshift(channel) mod(4).
      // Alas...
      switch (channel)
      {
      case 0:
        channel = 3;
        break;
      case 1:
        channel = 2;
        break;
      case 2:
        channel = 1;
        break;
      case 3:
        channel = 0;
        break;
      }
      if (state)
      {
        component_status |= (1u << channel);
      }
      else
      {
        component_status &= ~(1u << channel);
      }

      ESP_LOGD(TAG, "New status 0x%02X", component_status);

      if (!this->parent_->write_byte(this->address_, RELAY_CONTROL_REG, component_status))
      {
        return false;
      }

      return true;
    }

    void M5STACK4RELAYChannel::write_state(bool state)
    {

      if (!this->parent_->set_channel_value_(this->channel_, state))
      {
        publish_state(false);
      }
      else
      {
        publish_state(state);
      }
    }

  } // namespace m5stack_4relay
} // namespace esphome
