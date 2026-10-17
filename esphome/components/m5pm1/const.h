#pragma once

#include <cstdint>

namespace esphome::m5pm1 {

static constexpr uint8_t M5PM1_DEVICE_ID = 0x50;
static constexpr uint8_t M5PM1_DEVICE_MODEL = 0x20;
static constexpr uint8_t M5PM1_GPIO_COUNT = 5;
static constexpr uint8_t M5PM1_NEOPIXEL_COUNT = 32;
static constexpr uint8_t M5PM1_RTC_MEMORY_SIZE = 32;
static constexpr uint32_t M5PM1_TIMER_MAX_SECONDS = 214748364;

static constexpr uint8_t REG_DEVICE_ID = 0x00;
static constexpr uint8_t REG_DEVICE_MODEL = 0x01;
static constexpr uint8_t REG_HW_REV = 0x02;
static constexpr uint8_t REG_SW_REV = 0x03;
static constexpr uint8_t REG_PWR_SRC = 0x04;
static constexpr uint8_t REG_WAKE_SRC = 0x05;
static constexpr uint8_t REG_PWR_CFG = 0x06;
static constexpr uint8_t REG_HOLD_CFG = 0x07;
static constexpr uint8_t REG_BATT_LVP = 0x08;
static constexpr uint8_t REG_I2C_CFG = 0x09;
static constexpr uint8_t REG_WDT_CNT = 0x0A;
static constexpr uint8_t REG_WDT_KEY = 0x0B;
static constexpr uint8_t REG_SYS_CMD = 0x0C;

static constexpr uint8_t REG_GPIO_MODE = 0x10;
static constexpr uint8_t REG_GPIO_OUT = 0x11;
static constexpr uint8_t REG_GPIO_IN = 0x12;
static constexpr uint8_t REG_GPIO_DRV = 0x13;
static constexpr uint8_t REG_GPIO_PU_PD_0 = 0x14;
static constexpr uint8_t REG_GPIO_PU_PD_1 = 0x15;
static constexpr uint8_t REG_GPIO_FUNC_0 = 0x16;
static constexpr uint8_t REG_GPIO_FUNC_1 = 0x17;
static constexpr uint8_t REG_GPIO_WAKE_EN = 0x18;
static constexpr uint8_t REG_GPIO_WAKE_CFG = 0x19;

static constexpr uint8_t REG_ADC_VREF_L = 0x20;
static constexpr uint8_t REG_ADC_VBAT_L = 0x22;
static constexpr uint8_t REG_ADC_5V_IN_L = 0x24;
static constexpr uint8_t REG_ADC_5V_OUT_L = 0x26;
static constexpr uint8_t REG_ADC_RES_L = 0x28;
static constexpr uint8_t REG_ADC_CTRL = 0x2A;

static constexpr uint8_t REG_PWM_0_L = 0x30;
static constexpr uint8_t REG_PWM_1_L = 0x32;
static constexpr uint8_t REG_PWM_FREQ_L = 0x34;

static constexpr uint8_t REG_TIM_CNT_BYTE_0 = 0x38;
static constexpr uint8_t REG_TIM_CFG = 0x3C;
static constexpr uint8_t REG_TIM_KEY = 0x3D;

static constexpr uint8_t REG_IRQ_STAT_1 = 0x40;
static constexpr uint8_t REG_IRQ_STAT_2 = 0x41;
static constexpr uint8_t REG_IRQ_STAT_3 = 0x42;
static constexpr uint8_t REG_IRQ_STAT_1_MASK = 0x43;
static constexpr uint8_t REG_IRQ_STAT_2_MASK = 0x44;
static constexpr uint8_t REG_IRQ_STAT_3_MASK = 0x45;

static constexpr uint8_t REG_BTN_STAT = 0x48;
static constexpr uint8_t REG_BTN_CFG_1 = 0x49;
static constexpr uint8_t REG_BTN_CFG_2 = 0x4A;
static constexpr uint8_t REG_NEO_CFG = 0x50;
static constexpr uint8_t REG_AW8737A_CFG = 0x53;
static constexpr uint8_t REG_NEO_DATA = 0x60;
static constexpr uint8_t REG_RTC_DATA = 0xA0;

static constexpr uint8_t SYS_CMD_KEY = 0xA0;
static constexpr uint8_t WDT_KEY = 0xA5;
static constexpr uint8_t TIMER_KEY = 0xA5;
static constexpr uint8_t PWM_ENABLE = 1 << 4;
static constexpr uint8_t PWM_ACTIVE_LOW = 1 << 5;
static constexpr uint8_t NEO_REFRESH = 1 << 6;
static constexpr uint8_t AW8737A_REFRESH = 1 << 7;

static constexpr uint8_t M5PM1_IRQ_POWER_VIN_INSERTED = 1 << 0;
static constexpr uint8_t M5PM1_IRQ_POWER_VIN_REMOVED = 1 << 1;
static constexpr uint8_t M5PM1_IRQ_POWER_VINOUT_INSERTED = 1 << 2;
static constexpr uint8_t M5PM1_IRQ_POWER_VINOUT_REMOVED = 1 << 3;
static constexpr uint8_t M5PM1_IRQ_POWER_BATTERY_INSERTED = 1 << 4;
static constexpr uint8_t M5PM1_IRQ_POWER_BATTERY_REMOVED = 1 << 5;

static constexpr uint8_t M5PM1_IRQ_SYSTEM_SINGLE_CLICK = 1 << 0;
static constexpr uint8_t M5PM1_IRQ_SYSTEM_WAKEUP = 1 << 1;
static constexpr uint8_t M5PM1_IRQ_SYSTEM_DOUBLE_CLICK = 1 << 2;

static constexpr uint8_t M5PM1_WAKE_SOURCE_TIMER = 1 << 0;
static constexpr uint8_t M5PM1_WAKE_SOURCE_VIN = 1 << 1;
static constexpr uint8_t M5PM1_WAKE_SOURCE_POWER_BUTTON = 1 << 2;
static constexpr uint8_t M5PM1_WAKE_SOURCE_RESET_BUTTON = 1 << 3;
static constexpr uint8_t M5PM1_WAKE_SOURCE_COMMAND_RESET = 1 << 4;
static constexpr uint8_t M5PM1_WAKE_SOURCE_GPIO = 1 << 5;
static constexpr uint8_t M5PM1_WAKE_SOURCE_VINOUT = 1 << 6;

static constexpr uint8_t M5PM1_POWER_SOURCE_5V_IN_OUT = 1 << 1;

struct M5PM1DeviceInfo {
  uint8_t device_id;
  uint8_t device_model;
  uint8_t hardware_revision;
  uint8_t software_revision;
};

struct M5PM1IRQStatus {
  uint8_t gpio;
  uint8_t power;
  uint8_t system;
};

enum class M5PM1PinFunction : uint8_t {
  M5PM1_PIN_FUNCTION_GPIO = 0b00,
  M5PM1_PIN_FUNCTION_IRQ = 0b01,
  M5PM1_PIN_FUNCTION_RESERVED = 0b10,
  M5PM1_PIN_FUNCTION_ALTERNATE = 0b11,
};

enum class M5PM1PullMode : uint8_t {
  M5PM1_PULL_MODE_NONE = 0b00,
  M5PM1_PULL_MODE_UP = 0b01,
  M5PM1_PULL_MODE_DOWN = 0b10,
};

enum class M5PM1WakeupEdge : uint8_t {
  M5PM1_WAKEUP_EDGE_FALLING = 0,
  M5PM1_WAKEUP_EDGE_RISING = 1,
};

enum class M5PM1ADCChannel : uint8_t {
  M5PM1_ADC_CHANNEL_1 = 1,
  M5PM1_ADC_CHANNEL_2 = 2,
  M5PM1_ADC_CHANNEL_TEMPERATURE = 6,
};

enum class M5PM1PWMChannel : uint8_t {
  M5PM1_PWM_CHANNEL_1 = 0,
  M5PM1_PWM_CHANNEL_2 = 1,
};

enum class M5PM1PWMPolarity : uint8_t {
  M5PM1_PWM_POLARITY_ACTIVE_HIGH = 0,
  M5PM1_PWM_POLARITY_ACTIVE_LOW = 1,
};

enum class M5PM1TimerAction : uint8_t {
  M5PM1_TIMER_ACTION_STOP = 0,
  M5PM1_TIMER_ACTION_SET_WAKEUP_FLAG = 1,
  M5PM1_TIMER_ACTION_REBOOT = 2,
  M5PM1_TIMER_ACTION_POWER_ON = 3,
  M5PM1_TIMER_ACTION_POWER_OFF = 4,
};

enum class M5PM1SystemCommand : uint8_t {
  M5PM1_SYSTEM_COMMAND_POWER_OFF = 1,
  M5PM1_SYSTEM_COMMAND_REBOOT = 2,
  M5PM1_SYSTEM_COMMAND_DOWNLOAD = 3,
};

enum class M5PM1ButtonTiming : uint8_t {
  M5PM1_BUTTON_TIMING_125_MS = 0,
  M5PM1_BUTTON_TIMING_250_MS = 1,
  M5PM1_BUTTON_TIMING_500_MS = 2,
  M5PM1_BUTTON_TIMING_1000_MS = 3,
};

enum class M5PM1LongPressTiming : uint8_t {
  M5PM1_LONG_PRESS_TIMING_1_S = 0,
  M5PM1_LONG_PRESS_TIMING_2_S = 1,
  M5PM1_LONG_PRESS_TIMING_3_S = 2,
  M5PM1_LONG_PRESS_TIMING_4_S = 3,
};

enum M5PM1PowerConfig : uint8_t {
  M5PM1_POWER_CONFIG_CHARGER = 1 << 0,
  M5PM1_POWER_CONFIG_DCDC_3V3 = 1 << 1,
  M5PM1_POWER_CONFIG_LDO_3V3 = 1 << 2,
  M5PM1_POWER_CONFIG_5V_IN_OUT = 1 << 3,
  M5PM1_POWER_CONFIG_LED = 1 << 4,
};

enum M5PM1PowerHold : uint8_t {
  M5PM1_POWER_HOLD_GPIO_0 = 1 << 0,
  M5PM1_POWER_HOLD_GPIO_1 = 1 << 1,
  M5PM1_POWER_HOLD_GPIO_2 = 1 << 2,
  M5PM1_POWER_HOLD_GPIO_3 = 1 << 3,
  M5PM1_POWER_HOLD_GPIO_4 = 1 << 4,
  M5PM1_POWER_HOLD_LDO_3V3 = 1 << 5,
  M5PM1_POWER_HOLD_5V_IN_OUT = 1 << 6,
};

}  // namespace esphome::m5pm1
