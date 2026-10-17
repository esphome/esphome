from collections.abc import Callable

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_MODEL
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

MODEL_SEN0609 = "SEN0609"
MODEL_SEN0610 = "SEN0610"
# Longest detection range in metres
MAX_RANGES = {MODEL_SEN0609: 25.0, MODEL_SEN0610: 12.0}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DFRobotSEN0609Component),
            cv.Optional(CONF_MODEL, default=MODEL_SEN0609): cv.one_of(
                *MAX_RANGES, upper=True
            ),
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


def final_validate_mode(keys: dict[str, str]) -> Callable[[ConfigType], None]:
    """Reject entity keys that need a different hub mode; `keys` maps each key to the mode it needs."""

    def validator(config: ConfigType) -> None:
        hub = fv.get_declaring_config(config[CONF_DFROBOT_SEN0609_ID])
        for key, mode in keys.items():
            if key in config and hub[CONF_MODE] != mode:
                raise cv.Invalid(
                    f"'{key}' needs the {DOMAIN} 'mode' to be '{mode}'", path=[key]
                )

    return validator


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], RUN_MODES[config[CONF_MODE]])
    cg.add_define("USE_LINE_READER")
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
