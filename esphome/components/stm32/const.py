import esphome.codegen as cg

CONF_AF = "af"
CONF_MCU = KEY_MCU = "mcu"
CONF_MCU_SERIES = KEY_MCU_SERIES = "mcu_series"
CONF_FCPU = KEY_FCPU = "fcpu"
CONF_RAM = KEY_RAM = "ram"
CONF_ROM = KEY_ROM = "rom"

KEY_BOARD = "board"
KEY_STM32 = "stm32"
KEY_UART_INSTANCES = "uart_instances"
KEY_GPIO_CLOCK_ENABLED = "gpio_clock_enabled"


stm32_ns = cg.esphome_ns.namespace("stm32")
