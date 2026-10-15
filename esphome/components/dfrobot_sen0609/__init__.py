from collections.abc import Callable

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE
from esphome.core import ID
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@remcom"]
DEPENDENCIES = ["uart"]
DOMAIN = "dfrobot_sen0609"
MULTI_CONF = True

dfrobot_sen0609_ns = cg.esphome_ns.namespace("dfrobot_sen0609")
DFRobotSEN0609Component = dfrobot_sen0609_ns.class_(
    "DFRobotSEN0609Component", cg.Component, uart.UARTDevice
)
RunMode = dfrobot_sen0609_ns.enum("RunMode", is_class=True)

CONF_DFROBOT_SEN0609_ID = "dfrobot_sen0609_id"

MODE_PRESENCE = "presence"
MODE_SPEED_AND_DISTANCE = "speed_and_distance"
RUN_MODES = {
    MODE_PRESENCE: RunMode.RUN_MODE_PRESENCE,
    MODE_SPEED_AND_DISTANCE: RunMode.RUN_MODE_SPEED_AND_DISTANCE,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DFRobotSEN0609Component),
            cv.Optional(CONF_MODE, default=MODE_PRESENCE): cv.one_of(
                *RUN_MODES, lower=True
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    DOMAIN,
    baud_rate=9600,
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


def get_hub_config(root: ConfigType, hub_id: ID) -> ConfigType:
    """Return the configuration of the hub with the given id."""
    return next(conf for conf in root[DOMAIN] if conf[CONF_ID] == hub_id)


def final_validate_mode(keys: dict[str, str]) -> Callable[[ConfigType], None]:
    """Reject entity keys that need a different hub mode; `keys` maps each key to the mode it needs."""

    def validator(config: ConfigType) -> None:
        hub = get_hub_config(fv.full_config.get(), config[CONF_DFROBOT_SEN0609_ID])
        for key, mode in keys.items():
            if key in config and hub[CONF_MODE] != mode:
                raise cv.Invalid(
                    f"'{key}' needs the {DOMAIN} 'mode' to be '{mode}'", path=[key]
                )

    return validator


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], RUN_MODES[config[CONF_MODE]])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
