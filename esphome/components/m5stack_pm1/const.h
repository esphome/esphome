#pragma once

#include <cstdint>

namespace esphome::m5stack_pm1 {

static constexpr uint8_t M5STACK_PM1_DEVICE_ID = 0x50;
static constexpr uint8_t M5STACK_PM1_DEVICE_MODEL = 0x20;
static constexpr uint8_t M5STACK_PM1_GPIO_COUNT = 5;

static constexpr uint8_t REG_DEVICE_ID = 0x00;
static constexpr uint8_t REG_DEVICE_MODEL = 0x01;
static constexpr uint8_t REG_HW_REV = 0x02;
static constexpr uint8_t REG_SW_REV = 0x03;
static constexpr uint8_t REG_WAKE_SRC = 0x05;
static constexpr uint8_t REG_PWR_CFG = 0x06;
static constexpr uint8_t REG_I2C_CFG = 0x09;

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

static constexpr uint8_t REG_IRQ_STAT_1 = 0x40;
static constexpr uint8_t REG_IRQ_STAT_2 = 0x41;
static constexpr uint8_t REG_IRQ_STAT_3 = 0x42;
static constexpr uint8_t REG_IRQ_STAT_1_MASK = 0x43;
static constexpr uint8_t REG_IRQ_STAT_2_MASK = 0x44;
static constexpr uint8_t REG_IRQ_STAT_3_MASK = 0x45;

static constexpr uint8_t M5STACK_PM1_IRQ_SYSTEM_WAKEUP = 1 << 1;
static constexpr uint8_t M5STACK_PM1_I2C_CFG_SPD = 1 << 4;

struct M5StackPM1DeviceInfo {
  uint8_t device_id;
  uint8_t device_model;
  uint8_t hardware_revision;
  uint8_t software_revision;
};

struct M5StackPM1GPIORegs {
  uint8_t mode;
  uint8_t output;
  uint8_t input;
  uint8_t drive;
  uint8_t pull_0;
  uint8_t pull_1;
  uint8_t function_0;
  uint8_t function_1;
};

struct M5StackPM1IRQStatus {
  uint8_t gpio;
  uint8_t power;
  uint8_t system;
};

enum class M5StackPM1PinFunction : uint8_t {
  M5STACK_PM1_PIN_FUNCTION_GPIO = 0b00,
  M5STACK_PM1_PIN_FUNCTION_IRQ = 0b01,
  M5STACK_PM1_PIN_FUNCTION_RESERVED = 0b10,
  M5STACK_PM1_PIN_FUNCTION_ALTERNATE = 0b11,
};

enum class M5StackPM1PullMode : uint8_t {
  M5STACK_PM1_PULL_MODE_NONE = 0b00,
  M5STACK_PM1_PULL_MODE_UP = 0b01,
  M5STACK_PM1_PULL_MODE_DOWN = 0b10,
};

enum class M5StackPM1WakeupEdge : uint8_t {
  M5STACK_PM1_WAKEUP_EDGE_FALLING = 0,
  M5STACK_PM1_WAKEUP_EDGE_RISING = 1,
};

enum M5StackPM1PowerConfig : uint8_t {
  M5STACK_PM1_POWER_CONFIG_CHARGER = 1 << 0,
  M5STACK_PM1_POWER_CONFIG_DCDC_3V3 = 1 << 1,
  M5STACK_PM1_POWER_CONFIG_LDO_3V3 = 1 << 2,
  M5STACK_PM1_POWER_CONFIG_5V_IN_OUT = 1 << 3,
  M5STACK_PM1_POWER_CONFIG_LED = 1 << 4,
};

}  // namespace esphome::m5stack_pm1
