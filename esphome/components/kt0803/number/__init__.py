import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    DEVICE_CLASS_FREQUENCY,
    ENTITY_CATEGORY_CONFIG,
)
from .. import (
    CONF_KT0803_ID,
    KT0803Component,
    kt0803_ns,
    UNIT_MEGA_HERTZ,
)

FrequencyNumber = kt0803_ns.class_("FrequencyNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_KT0803_ID): cv.use_id(KT0803Component),
        cv.Optional(CONF_FREQUENCY): number.number_schema(
            FrequencyNumber,
            unit_of_measurement=UNIT_MEGA_HERTZ,
            device_class=DEVICE_CLASS_FREQUENCY,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def new_number(config, id, setter, min_value, max_value, step):
    if c := config.get(id):
        n = await number.new_number(c, min_value=min_value, max_value=max_value, step=step)
        await cg.register_parented(n, config[CONF_KT0803_ID])
        cg.add(setter(n))

async def to_code(config):
    kt0803_component = await cg.get_variable(config[CONF_KT0803_ID])
    await new_number(config, CONF_FREQUENCY, kt0803_component.set_frequency_number, 70, 108, 0.05)