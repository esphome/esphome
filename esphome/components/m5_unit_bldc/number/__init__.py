import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, ICON_ROTATE_RIGHT
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_M5_UNIT_BLDC_ID, DOMAIN, UNIT_RPM, M5UnitBldc, m5_unit_bldc_ns

CONF_PWM = "pwm"
CONF_TARGET_RPM = "target_rpm"

M5UnitBldcNumber = m5_unit_bldc_ns.class_(
    "M5UnitBldcNumber", number.Number, cg.Parented.template(M5UnitBldc)
)
NumberType = m5_unit_bldc_ns.enum("NumberType", is_class=True)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_M5_UNIT_BLDC_ID): cv.use_id(M5UnitBldc),
    # Open-loop control -- raw PWM duty cycle. Only takes effect while the hub's `mode` is `open_loop`.
    cv.Optional(CONF_PWM): number.number_schema(
        M5UnitBldcNumber,
        icon=ICON_ROTATE_RIGHT,
    ),
    # Closed-loop control -- target RPM. Only takes effect while the hub's `mode` is `closed_loop`.
    cv.Optional(CONF_TARGET_RPM): number.number_schema(
        M5UnitBldcNumber,
        icon=ICON_ROTATE_RIGHT,
        unit_of_measurement=UNIT_RPM,
    ),
}


def _final_validate(config: ConfigType) -> None:
    full_config = fv.full_config.get()
    hub_id = config[CONF_M5_UNIT_BLDC_ID]
    hub_config = next(
        (entry for entry in full_config[DOMAIN] if entry[CONF_ID] == hub_id), None
    )
    if hub_config is None:
        return

    # `cv.enum` keeps the validated config value as the lowered option string itself (with the
    # generated C++ expression attached as a side attribute for code generation), so a plain
    # string comparison is enough here.
    is_closed_loop = hub_config[CONF_MODE] == "closed_loop"

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


async def to_code(config):
    parent = await cg.get_variable(config[CONF_M5_UNIT_BLDC_ID])

    if pwm_config := config.get(CONF_PWM):
        num = await number.new_number(
            pwm_config, NumberType.NUMBER_TYPE_PWM, min_value=0, max_value=2047, step=1
        )
        await cg.register_parented(num, parent)
        cg.add(parent.set_pwm_number(num))

    if target_rpm_config := config.get(CONF_TARGET_RPM):
        num = await number.new_number(
            target_rpm_config,
            NumberType.NUMBER_TYPE_TARGET_RPM,
            min_value=0,
            max_value=20000,
            step=1,
        )
        await cg.register_parented(num, parent)
        cg.add(parent.set_target_rpm_number(num))
