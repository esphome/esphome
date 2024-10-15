import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    UNIT_PERCENT,
    UNIT_DECIBEL,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_EMPTY,
    ENTITY_CATEGORY_CONFIG,
)
from .. import (
    CONF_SI4713_ID,
    Si4713Component,
    si4713_ns,
#    CONF_,
    UNIT_MEGA_HERTZ,
    UNIT_KILO_HERTZ,
    UNIT_MICRO_AMPERE,
    UNIT_DECIBEL_MICRO_VOLT,
)

FrequencyNumber = si4713_ns.class_("FrequencyNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SI4713_ID): cv.use_id(Si4713Component),
        cv.Optional(CONF_FREQUENCY): number.number_schema(
            FrequencyNumber,
            unit_of_measurement=UNIT_MEGA_HERTZ,
            device_class=DEVICE_CLASS_FREQUENCY,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def new_number(config, id, setter, min_value, max_value, step, *args):
    if c := config.get(id):
        n = await number.new_number(
            c, *args, min_value=min_value, max_value=max_value, step=step
        )
        await cg.register_parented(n, config[CONF_SI4713_ID])
        cg.add(setter(n))


async def to_code(config):
    c = await cg.get_variable(config[CONF_SI4713_ID])
    await new_number(config, CONF_FREQUENCY, c.set_frequency_number, 76, 108, 0.05)
