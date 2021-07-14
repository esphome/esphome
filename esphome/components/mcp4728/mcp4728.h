#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/log.h"
#include <Adafruit_MCP4728.h>
// #include <Wire.h>
// #include <SPI.h>

namespace esphome
{
  namespace mcp4728
  {
    Adafruit_MCP4728 mcp;
    static const char *const TAG = "mcp4728";

    class MCP4728OutputComponent;

    class MCP4728Channel : public output::FloatOutput
    {
    public:
      MCP4728Channel(MCP4728OutputComponent *parent, uint8_t channel) : parent_(parent), channel_(channel) {}

    protected:
      void write_state(float state) override
      {
        MCP4728_channel_t channel_enum;
        uint16_t value = (uint16_t)round(state * (pow(2, 12) - 1));
        switch (this->channel_)
        {
        case 0:
          channel_enum = MCP4728_CHANNEL_A;
          break;
        case 1:
          channel_enum = MCP4728_CHANNEL_B;
          break;
        case 2:
          channel_enum = MCP4728_CHANNEL_C;
          break;
        case 3:
          channel_enum = MCP4728_CHANNEL_D;
          break;
        }

        mcp.setChannelValue(
            channel_enum,
            value,
            MCP4728_VREF_VDD,
            MCP4728_GAIN_2X,
            MCP4728_PD_MODE_NORMAL,
            false);
      };

      MCP4728OutputComponent *parent_;
      uint8_t channel_;
    };

    class MCP4728OutputComponent : public Component, public i2c::I2CDevice
    {
    public:
      MCP4728OutputComponent() {}
      MCP4728Channel *create_channel(uint8_t channel)
      {
        auto *c = new MCP4728Channel(this, channel);
        return c;
      }

      void setup() override
      {
        ESP_LOGCONFIG(TAG, "Setting up MCP4728OutputComponent...");
        if (!mcp.begin())
        {
          ESP_LOGE(TAG, "Communication with MCP4725 failed!");
        }
      };
      float get_setup_priority() const override { return setup_priority::HARDWARE; }

    protected:
      friend MCP4728Channel;
      float value_;
    };

  } // namespace mcp4728
} // namespace esphome
