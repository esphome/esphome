import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ILLUMINANCE,
    CONF_SAMPLE_RATE,
    CONF_TYPE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
)

from . import APDS9930, CONF_APDS9930_ID

DEPENDENCIES = ["apds9930"]

TYPES = [CONF_ILLUMINANCE, "proximity"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(cg.Component),
        cv.GenerateID(CONF_APDS9930_ID): cv.use_id(APDS9930),
        cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_LUX,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("proximity"): sensor.sensor_schema(
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    },
).extend(
    {
        cv.Required(CONF_TYPE): cv.one_of(*TYPES, lower=True),
    }
)


async def setup_conf(config, key, hub):
    if conf := config.get(key):
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))
        if sample_rate := conf.get(CONF_SAMPLE_RATE):
            cg.add(getattr(hub, f"set_{key}_sample_rate")(sample_rate))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_APDS9930_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
    # var = await sensor.new_sensor(config)
    # func = getattr(hub, f"set_{config[CONF_TYPE]}_sensor")
    # cg.add(func(var))
