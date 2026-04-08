import esphome.codegen as cg
from esphome.components.logger import request_log_listener
from esphome.components.uart import (
    UARTComponent,
    debug_to_code,
    maybe_empty_debug,
    uart_ns,
)
from esphome.components.zephyr import zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEBUG,
    CONF_ID,
    CONF_LOGS,
    CONF_RX_BUFFER_SIZE,
    CONF_TX_BUFFER_SIZE,
    CONF_TYPE,
)
from esphome.types import ConfigType

AUTO_LOAD = ["zephyr_ble_server", "uart"]
CODEOWNERS = ["@tomaszduda23"]

ble_nus_ns = cg.esphome_ns.namespace("ble_nus")
BLENUS = ble_nus_ns.class_("BLENUS", cg.Component, UARTComponent)

CONF_UART = "uart"


def validate_rx_buffer(config: ConfigType) -> ConfigType:
    config = config.copy()
    if config[CONF_TYPE] == CONF_LOGS:
        if CONF_RX_BUFFER_SIZE in config:
            raise cv.Invalid("logs does not support rx_buffer_size")
    elif CONF_RX_BUFFER_SIZE not in config:
        config[CONF_RX_BUFFER_SIZE] = 512
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLENUS),
            cv.Optional(CONF_TYPE, default=CONF_LOGS): cv.one_of(
                *[CONF_LOGS, CONF_UART], lower=True
            ),
            cv.Optional(CONF_TX_BUFFER_SIZE, default=512): cv.All(
                cv.validate_bytes, cv.int_range(min=160, max=8192)
            ),
            cv.Optional(CONF_RX_BUFFER_SIZE): cv.All(
                cv.validate_bytes, cv.int_range(min=160, max=8192)
            ),
            cv.Optional(CONF_DEBUG): maybe_empty_debug,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_framework("zephyr"),
    validate_rx_buffer,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    zephyr_add_prj_conf("BT_NUS", True)
    expose_log = config[CONF_TYPE] == CONF_LOGS
    cg.add(var.set_expose_log(expose_log))
    if expose_log:
        request_log_listener()  # Request a log listener slot for BLE NUS log streaming
    await cg.register_component(var, config)
    cg.add_define("ESPHOME_BLE_NUS_TX_RING_BUFFER_SIZE", config[CONF_TX_BUFFER_SIZE])
    if CONF_RX_BUFFER_SIZE in config:
        cg.add_define(
            "ESPHOME_BLE_NUS_RX_RING_BUFFER_SIZE", config[CONF_RX_BUFFER_SIZE]
        )
    if CONF_DEBUG in config:
        cg.add_global(uart_ns.using)
        await debug_to_code(config[CONF_DEBUG], var)
