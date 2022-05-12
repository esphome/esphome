from esphome.components import select
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_OPTIONS,
    CONF_OPTIMISTIC,
    CONF_ID,
    CONF_SELECT_DATAPOINT
)
from .. import tuya_ns, CONF_TUYA_ID, Tuya

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@bearpawmaxim"]

TuyaSelect = tuya_ns.class_("TuyaSelect", select.Select, cg.Component)

CONFIG_SCHEMA = select.SELECT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TuyaSelect),
        cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
        cv.Required(CONF_SELECT_DATAPOINT): cv.uint8_t,
        cv.Required(CONF_OPTIONS): cv.All(
                cv.ensure_list(cv.string_strict), cv.Length(min=1)
            ),
        cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await select.register_select(var, config, options=config[CONF_OPTIONS])

    parent = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(parent))
    cg.add(var.set_select_id(config[CONF_SELECT_DATAPOINT]))
    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
