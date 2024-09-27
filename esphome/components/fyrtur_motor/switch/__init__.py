import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
    ICON_ARROW_EXPAND_VERTICAL,
)
from .. import CONF_FYRTUR_MOTOR_ID, FyrturMotorComponent, fyrtur_motor_ns

OpenCloseSwitch = fyrtur_motor_ns.class_("OpenCloseSwitch", switch.Switch)

CONF_OPEN_CLOSE = "open_close"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_FYRTUR_MOTOR_ID): cv.use_id(FyrturMotorComponent),
    cv.Required(CONF_OPEN_CLOSE): switch.switch_schema(
        OpenCloseSwitch,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_ARROW_EXPAND_VERTICAL,
    ),
}


async def to_code(config):
    fyrtur_motor_component = await cg.get_variable(config[CONF_FYRTUR_MOTOR_ID])
    if open_close_config := config.get(CONF_OPEN_CLOSE):
        s = await switch.new_switch(open_close_config)
        await cg.register_parented(s, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.set_open_close_switch(s))
