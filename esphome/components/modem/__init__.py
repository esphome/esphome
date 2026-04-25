import logging

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import uart
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
from esphome.components.text_sensor import new_text_sensor, text_sensor_schema
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_ENABLE_ON_BOOT,
    CONF_ID,
    CONF_MODEL,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_PASSWORD,
    CONF_REBOOT_TIMEOUT,
    CONF_RX_BUFFER_SIZE,
    CONF_RX_PIN,
    CONF_TRIGGER_ID,
    CONF_TX_BUFFER_SIZE,
    CONF_TX_PIN,
    CONF_USE_ADDRESS,
    CONF_USERNAME,
    DEVICE_CLASS_EMPTY,
)
from esphome.core import coroutine_with_priority

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@oarcher"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["network", "uart", "text_sensor"]
CONFLICTS_WITH = ["captive_portal", "ethernet"]

CONF_MODEM = "modem"
CONF_MODEM_ID = "modem_id"
CONF_PIN_CODE = "pin_code"
CONF_APN = "apn"
CONF_DTR_PIN = "dtr_pin"
CONF_RTS_PIN = "rts_pin"
CONF_CTS_PIN = "cts_pin"
CONF_INIT_AT = "init_at"
CONF_ON_SYNC = "on_sync"
CONF_DTE_BUFFER_SIZE = "dte_buffer_size"
CONF_URC = "urc"

MODEM_MODELS = [
    "BG96",
    "SIM800",
    "SIM7000",
    "SIM7070",
    "SIM7080",
    "SIM7600",
    "SIM7670",
    "GENERIC",
]
modem_ns = cg.esphome_ns.namespace("modem")
ModemComponent = modem_ns.class_("ModemComponent", cg.Component, uart.UARTComponent)

# Triggers
ModemComponentState = modem_ns.enum("ModemComponentState")
ModemOnConnectTrigger = modem_ns.class_(
    "ModemOnConnectTrigger", automation.Trigger.template()
)
ModemOnDisconnectTrigger = modem_ns.class_(
    "ModemOnDisconnectTrigger", automation.Trigger.template()
)
ModemOnSyncTrigger = modem_ns.class_(
    "ModemOnSyncTrigger", automation.Trigger.template()
)

# Actions
ModemEnableAction = modem_ns.class_("ModemEnableAction", automation.Action)
ModemDisableAction = modem_ns.class_("ModemDisableAction", automation.Action)
ModemResetAction = modem_ns.class_("ModemResetAction", automation.Action)
ModemSendAtAction = modem_ns.class_("ModemSendAtAction", automation.Action)

# Conditions
ModemConnectedCondition = modem_ns.class_(
    "ModemConnectedCondition", automation.Condition.template()
)
ModemEnabledCondition = modem_ns.class_(
    "ModemEnabledCondition", automation.Condition.template()
)


MODEM_COMPONENT_SCHEMA = cv.Schema(
    {cv.GenerateID(CONF_MODEM_ID): cv.use_id(ModemComponent)}
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(ModemComponent),
            cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_RX_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RTS_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_CTS_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_BAUD_RATE): cv.positive_int,
            cv.Required(CONF_MODEL): cv.one_of(*MODEM_MODELS, upper=True),
            cv.Required(CONF_APN): cv.string,
            cv.Optional(CONF_PIN_CODE): cv.string_strict,
            cv.Optional(CONF_USE_ADDRESS): cv.string,
            cv.Optional(CONF_INIT_AT): cv.All(cv.ensure_list(cv.string)),
            cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_TX_BUFFER_SIZE, default=1024): cv.positive_int,
            cv.Optional(CONF_RX_BUFFER_SIZE, default=1024): cv.positive_int,
            cv.Optional(CONF_DTE_BUFFER_SIZE, default=1024): cv.positive_int,
            cv.Optional(
                CONF_REBOOT_TIMEOUT, default="10min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ModemOnConnectTrigger)}
            ),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ModemOnDisconnectTrigger
                    )
                }
            ),
            cv.Optional(CONF_ON_SYNC): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ModemOnSyncTrigger)}
            ),
            cv.Optional(CONF_URC): text_sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
            ),
        }
    )
    .extend(MODEM_COMPONENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(
        esp_idf=cv.Version(4, 0, 0),  # 5.2.0 OK
    ),
)


# Register actions


@automation.register_action(
    "modem.enable", ModemEnableAction, cv.Schema({}), synchronous=True
)
async def modem_enable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)


@automation.register_action(
    "modem.disable", ModemDisableAction, cv.Schema({}), synchronous=True
)
async def modem_disable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)


@automation.register_action(
    "modem.send_at", ModemSendAtAction, cv.templatable(cv.string), synchronous=True
)
async def modem_send_at_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    template_ = await cg.templatable(config, args, cg.std_string)
    cg.add(var.set_command(template_))
    return var


# Register conditions


@automation.register_condition(
    "modem.connected",
    ModemConnectedCondition,
    cv.Schema({}),
)
async def modem_connected_to_code(config, condition_id, template_arg, args):
    return cg.new_Pvariable(condition_id, template_arg)


@automation.register_condition(
    "modem.enabled",
    ModemEnabledCondition,
    cv.Schema({}),
)
async def modem_enabled_to_code(config, condition_id, template_arg, args):
    return cg.new_Pvariable(condition_id, template_arg)


@coroutine_with_priority(60.0)
async def to_code(config):
    add_idf_component(
        name="esp_modem",
        repo="https://github.com/espressif/esp-protocols.git",
        ref="modem-v2.0.0",
        path="components/esp_modem",
    )

    add_idf_sdkconfig_option("CONFIG_PPP", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_PPP_SUPPORT", True)
    add_idf_sdkconfig_option("CONFIG_ESP_MODEM_CMUX_DELAY_AFTER_DLCI_SETUP", 500)
    add_idf_sdkconfig_option("CONFIG_PPP_SUPPORT", True)
    add_idf_sdkconfig_option(
        "CONFIG_ESP_MODEM_CMUX_USE_SHORT_PAYLOADS_ONLY", True
    )  # Slower but more reliable
    add_idf_sdkconfig_option("CONFIG_ESP_MODEM_URC_HANDLER", True)

    # If "Uart queue full" messages appear (e.g., with A7672), these config options might need adjustment
    # See: https://github.com/espressif/esp-protocols/issues/272#issuecomment-1558682967
    # add_idf_sdkconfig_option("CONFIG_ESP_MODEM_CMUX_DEFRAGMENT_PAYLOAD", True)
    # add_idf_sdkconfig_option("CONFIG_ESP_MODEM_USE_INFLATABLE_BUFFER_IF_NEEDED", True)
    # add_idf_sdkconfig_option("CONFIG_ESP_MODEM_CMUX_USE_SHORT_PAYLOADS_ONLY", False)

    cg.add_define("USE_MODEM")

    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))

    if use_address := config.get(CONF_USE_ADDRESS, None):
        cg.add(var.set_use_address(use_address))

    if username := config.get(CONF_USERNAME, None):
        cg.add(var.set_username(username))

    if password := config.get(CONF_PASSWORD, None):
        cg.add(var.set_password(password))

    if pin_code := config.get(CONF_PIN_CODE, None):
        cg.add(var.set_pin_code(pin_code))

    if config[CONF_ENABLE_ON_BOOT]:
        cg.add(var.enable())

    if init_at := config.get(CONF_INIT_AT, None):
        for cmd in init_at:
            cg.add(var.add_init_at_command(cmd))

    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_apn(config[CONF_APN]))

    tx_pin = await cg.gpio_pin_expression(config[CONF_TX_PIN])
    cg.add(var.set_tx_pin(tx_pin))

    rx_pin = await cg.gpio_pin_expression(config[CONF_RX_PIN])
    cg.add(var.set_rx_pin(rx_pin))

    if rts_pin := config.get(CONF_RTS_PIN, None):
        rts_pin_expr = await cg.gpio_pin_expression(rts_pin)
        cg.add(var.set_rts_pin(rts_pin_expr))

    if cts_pin := config.get(CONF_CTS_PIN, None):
        cts_pin_expr = await cg.gpio_pin_expression(cts_pin)
        cg.add(var.set_cts_pin(cts_pin_expr))

    if baud_rate := config.get(CONF_BAUD_RATE, None):
        cg.add(var.set_baud_rate(baud_rate))

    if rx_buffer_size := config.get(CONF_RX_BUFFER_SIZE, None):
        cg.add(var.set_rx_buffer_size(rx_buffer_size))

    if tx_buffer_size := config.get(CONF_TX_BUFFER_SIZE, None):
        cg.add(var.set_tx_buffer_size(tx_buffer_size))

    if dte_buffer_size := config.get(CONF_DTE_BUFFER_SIZE, None):
        cg.add(var.set_dte_buffer_size(dte_buffer_size))

    for conf_key in [
        CONF_ON_CONNECT,
        CONF_ON_DISCONNECT,
        CONF_ON_SYNC,
    ]:
        for conf in config.get(conf_key, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, [], conf)

    if urc_config := config.get(CONF_URC, None):
        cg.add_define("USE_MODEM_URC")
        urc_text_sensor = await new_text_sensor(urc_config)
        cg.add(var.set_urc_text_sensor(urc_text_sensor))

    await cg.register_component(var, config)
