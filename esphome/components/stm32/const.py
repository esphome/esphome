import esphome.codegen as cg

KEY_BOARD = "board"
KEY_MCU = "mcu"
KEY_MCU_SERIES = "mcu_series"
KEY_FCPU = "fcpu"
KEY_RAM = "ram"
KEY_ROM = "rom"
KEY_STM32 = "stm32"
KEY_UART_INSTANCES = "uart_instances"
KEY_GPIO_CLOCK_ENABLED = "gpio_clock_enabled"

CONF_AF = "af"

stm32_ns = cg.esphome_ns.namespace("stm32")
