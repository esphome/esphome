from esphome.components import select
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_OPTIONS,
    CONF_SELECT_DATAPOINT,
    CONF_NAME,
    CONF_VALUE,
)
from .. import tuya_ns, CONF_TUYA_ID, Tuya

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@jkolo"]

TuyaSelect = tuya_ns.class_("TuyaSelect", select.Select, cg.Component)

CONFIG_SCHEMA = select.SELECT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TuyaSelect),
        cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
        cv.Required(CONF_SELECT_DATAPOINT): cv.uint8_t,
        cv.Required(CONF_OPTIONS): cv.All(
            cv.ensure_list(
                cv.ensure_schema(
                    cv.Schema(
                        {
                            cv.Required(CONF_NAME): cv.string_strict,
                            cv.Required(CONF_VALUE): cv.uint8_t,
                        }
                    )
                )
            ),
            cv.Length(min=1),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await select.register_select(
        var, config, options=[o[CONF_NAME] for o in config[CONF_OPTIONS]]
    )

    paren = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))
    cg.add(var.set_enums([o[CONF_VALUE] for o in config[CONF_OPTIONS]]))

    cg.add(var.set_select_id(config[CONF_SELECT_DATAPOINT]))
