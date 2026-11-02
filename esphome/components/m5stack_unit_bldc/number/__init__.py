import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_MODE, ICON_ROTATE_RIGHT, UNIT_REVOLUTIONS_PER_MINUTE
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_M5STACK_UNIT_BLDC_ID, M5StackUnitBldc

CONF_PWM = "pwm"
CONF_TARGET_RPM = "target_rpm"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_M5STACK_UNIT_BLDC_ID): cv.use_id(M5StackUnitBldc),
    cv.Optional(CONF_PWM): number.number_schema(
        number.ApplyNumber,
        icon=ICON_ROTATE_RIGHT,
    ),
    cv.Optional(CONF_TARGET_RPM): number.number_schema(
        number.ApplyNumber,
        icon=ICON_ROTATE_RIGHT,
        unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
    ),
}


def _final_validate(config: ConfigType) -> None:
    is_closed_loop = (
        fv.get_declaring_config(config[CONF_M5STACK_UNIT_BLDC_ID])[CONF_MODE]
        == "closed_loop"
    )
    if CONF_PWM in config and is_closed_loop:
        raise cv.Invalid(
            f"'{CONF_PWM}' has no effect while the hub's mode is 'closed_loop'; "
            f"use '{CONF_TARGET_RPM}' instead",
            path=[CONF_PWM],
        )
    if CONF_TARGET_RPM in config and not is_closed_loop:
        raise cv.Invalid(
            f"'{CONF_TARGET_RPM}' has no effect while the hub's mode is 'open_loop'; "
            f"use '{CONF_PWM}' instead",
            path=[CONF_TARGET_RPM],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5STACK_UNIT_BLDC_ID])
    # Validation allows only the key that matches the hub's mode, so both share the command setter
    numbers = number.sub_numbers(config, parent=hub)
    await numbers(
        CONF_PWM,
        hub.set_command_number,
        apply="control_command(value)",
        min_value=0,
        max_value=2047,
        step=1,
    )
    await numbers(
        CONF_TARGET_RPM,
        hub.set_command_number,
        apply="control_command(value)",
        min_value=0,
        max_value=20000,
        step=1,
    )
