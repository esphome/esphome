import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from .. import (
    CONF_EBUS_ID,
    CONF_TELEGRAM,
    CONF_DECODE,
    ebus_ns,
    create_telegram_schema,
    item_config,
)

AUTO_LOAD = ["ebus"]

EbusBinarySensor = ebus_ns.class_(
    "EbusBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONF_MASK = "mask"

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(EbusBinarySensor).extend(
        create_telegram_schema(
            {cv.Optional(CONF_MASK, default=0xFF): cv.int_range(1, 255)}
        )
    )
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    ebus = await cg.get_variable(config[CONF_EBUS_ID])
    sens = await binary_sensor.new_binary_sensor(config)

    item_config(ebus, sens, config)
    cg.add(sens.set_response_read_mask(config[CONF_TELEGRAM][CONF_DECODE][CONF_MASK]))
