from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_SWITCH_DATAPOINT
from .. import tuya_ns, CONF_TUYA_ID, Tuya

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@jesserockz"]

TuyaSwitch = tuya_ns.class_("TuyaSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TuyaSwitch),
        cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
        cv.Required(CONF_SWITCH_DATAPOINT): cv.uint8_t,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    paren = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))

    cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
