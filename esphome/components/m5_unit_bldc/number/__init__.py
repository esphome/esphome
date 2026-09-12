import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import ICON_ROTATE_RIGHT

from .. import CONF_M5_UNIT_BLDC_ID, UNIT_RPM, M5UnitBldc, m5_unit_bldc_ns

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


async def to_code(config):
    parent = await cg.get_variable(config[CONF_M5_UNIT_BLDC_ID])

    if pwm_config := config.get(CONF_PWM):
        num = await number.new_number(
            pwm_config, NumberType.PWM, min_value=0, max_value=2047, step=1
        )
        await cg.register_parented(num, parent)

    if target_rpm_config := config.get(CONF_TARGET_RPM):
        num = await number.new_number(
            target_rpm_config,
            NumberType.TARGET_RPM,
            min_value=0,
            max_value=20000,
            step=1,
        )
        await cg.register_parented(num, parent)
