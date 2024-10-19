from esphome import pins
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import (
    CONF_DOMAIN,
    CONF_ID,
    CONF_RESET_PIN,
    CONF_RX_BUFFER_SIZE,
    CONF_RX_PIN,
    CONF_TX_BUFFER_SIZE,
    CONF_TX_PIN,
    CONF_TYPE,
    CONF_USE_ADDRESS,
)
from esphome.core import CORE, coroutine_with_priority

CONFLICTS_WITH = ["wifi", "ethernet"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["network"]

modem_ns = cg.esphome_ns.namespace("modem")
CONF_APN = "apn"
CONF_UART_EVENT_TASK_STACK_SIZE = "uart_event_task_stack_size"
CONF_UART_EVENT_TASK_PRIORITY = "uart_event_task_priority"
CONF_UART_EVENT_QUEUE_SIZE = "uart_event_queue_size"
CONF_POWER_PIN = "power_pin"
CONF_PWRKEY_PIN = "pwrkey_pin"

ModemType = modem_ns.enum("ModemType")
MODEM_TYPES = {
    "BG96": ModemType.MODEM_TYPE_BG96,
    "SIM800": ModemType.MODEM_TYPE_SIM800,
    "SIM7000": ModemType.MODEM_TYPE_SIM7000,
    "SIM7070": ModemType.MODEM_TYPE_SIM7070,
}

ModemComponent = modem_ns.class_("ModemComponent", cg.Component)


def _validate(config):
    if CONF_USE_ADDRESS not in config:
        use_address = CORE.name + config[CONF_DOMAIN]
        config[CONF_USE_ADDRESS] = use_address
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModemComponent),
            cv.Required(CONF_TYPE): cv.enum(MODEM_TYPES, upper=True),
            cv.Optional(CONF_POWER_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_PWRKEY_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_RX_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_APN, default="internet"): cv.string,
            cv.Optional(
                CONF_UART_EVENT_TASK_STACK_SIZE, default=2048
            ): cv.positive_not_null_int,
            cv.Optional(CONF_UART_EVENT_TASK_PRIORITY, default=5): cv.positive_not_null_int,
            cv.Optional(CONF_UART_EVENT_QUEUE_SIZE, default=30): cv.positive_not_null_int,
            cv.Optional(CONF_TX_BUFFER_SIZE, default=512): cv.positive_not_null_int,
            cv.Optional(CONF_RX_BUFFER_SIZE, default=1024): cv.positive_not_null_int,
            cv.Optional(CONF_DOMAIN, default=".local"): cv.domain_name,
            cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
)


@coroutine_with_priority(60.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_type(config[CONF_TYPE]))
    if (
        config.get(CONF_RESET_PIN, None)
        or config.get(CONF_POWER_PIN, None)
        or config.get(CONF_PWRKEY_PIN, None)
    ):
        if reset_pin := config.get(CONF_RESET_PIN, None):
            pin = await cg.gpio_pin_expression(reset_pin)
            cg.add(var.set_reset_pin(pin))
        if power_pin := config.get(CONF_POWER_PIN, None):
            pin = await cg.gpio_pin_expression(power_pin)
            cg.add(var.set_power_pin(pin))
        if pwrkey_pin := config.get(CONF_PWRKEY_PIN, None):
            pin = await cg.gpio_pin_expression(pwrkey_pin)
            cg.add(var.set_pwrkey_pin(pin))
    else:
        raise NotImplementedError(
            "Modem component requires at least a reset pin or a power pin or a pwrkey pin"
        )

    cg.add(var.set_tx_pin(config[CONF_TX_PIN]))
    cg.add(var.set_rx_pin(config[CONF_RX_PIN]))
    cg.add(var.set_apn(config[CONF_APN]))
    cg.add(var.set_uart_tx_buffer_size(config[CONF_TX_BUFFER_SIZE]))
    cg.add(var.set_uart_rx_buffer_size(config[CONF_RX_BUFFER_SIZE]))
    cg.add(var.set_uart_event_task_stack_size(config[CONF_UART_EVENT_TASK_STACK_SIZE]))
    cg.add(var.set_uart_event_task_priority(config[CONF_UART_EVENT_TASK_PRIORITY]))
    cg.add(var.set_uart_event_queue_size(config[CONF_UART_EVENT_QUEUE_SIZE]))
    cg.add(var.set_use_address(config[CONF_USE_ADDRESS]))
    cg.add_define("USE_MODEM")
    if CORE.using_arduino:
        cg.add_library("WiFi", None)
    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_LWIP_PPP_SUPPORT", True)
        # add_idf_sdkconfig_option("CONFIG_TASK_WDT_TIMEOUT_S", 60)
        # add_idf_sdkconfig_option("CONFIG_LOG_MAXIMUM_EQUALS_DEFAULT", False)
        # add_idf_sdkconfig_option("CONFIG_LOG_MAXIMUM_LEVEL_VERBOSE", True)
        add_idf_component(
            name="esp_modem",
            repo="https://github.com/espressif/esp-protocols.git",
            ref="modem-v1.1.0",
            path="components/esp_modem",
        )
