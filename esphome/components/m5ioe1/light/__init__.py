import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT_ID

from .. import BASE_SCHEMA, CONF_M5IOE1_ID, m5ioe1_ns

M5IOE1AddressableLight = m5ioe1_ns.class_(
    "M5IOE1AddressableLight", light.AddressableLight
)

CONF_NUM_LED = "num_led"

CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(M5IOE1AddressableLight),
            cv.Required(CONF_NUM_LED): cv.Range(min=0, max=32),
        }
    ).extend(BASE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_parented(var, config[CONF_M5IOE1_ID])
    await cg.register_component(var, config)

    cg.add(var.set_num_led(config[CONF_NUM_LED]))
