import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor

from esphome.const import (
    CONF_BYTES,
)

from .. import (
    CONF_EBUS_ID,
    CONF_TELEGRAM,
    CONF_DECODE,
    ebus_ns,
    create_telegram_schema,
    item_config,
)

AUTO_LOAD = ["ebus"]

EbusSensor = ebus_ns.class_("EbusSensor", sensor.Sensor, cg.Component)

CONF_DIVIDER = "divider"

CONFIG_SCHEMA = (
    sensor.sensor_schema(EbusSensor).extend(
        create_telegram_schema(
            {
                cv.Required(CONF_BYTES): cv.int_range(1, 4),
                cv.Required(CONF_DIVIDER): cv.float_,
            }
        )
    )
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    ebus = await cg.get_variable(config[CONF_EBUS_ID])
    sens = await sensor.new_sensor(config)

    item_config(ebus, sens, config)

    cg.add(sens.set_response_read_bytes(config[CONF_TELEGRAM][CONF_DECODE][CONF_BYTES]))
    cg.add(
        sens.set_response_read_divider(config[CONF_TELEGRAM][CONF_DECODE][CONF_DIVIDER])
    )
