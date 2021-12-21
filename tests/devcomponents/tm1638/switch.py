import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID, CONF_LED

CONF_TM1638_ID = "tm1638_id"

tm1638_ns = cg.esphome_ns.namespace("tm1638")
TM1638Component = tm1638_ns.class_("TM1638Component", cg.PollingComponent)
TM1638Led = tm1638_ns.class_("TM1638Led", switch.Switch)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TM1638Led),
        cv.GenerateID(CONF_TM1638_ID): cv.use_id(TM1638Component),
        cv.Required(CONF_LED): cv.int_range(min=0, max=7),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await switch.register_switch(var, config)
    cg.add(var.set_lednum(config[CONF_LED]))
    hub = await cg.get_variable(config[CONF_TM1638_ID])
    cg.add(hub.add_tm1638_led(var))
