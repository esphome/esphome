import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_DALLAS_ID,
    CONF_INDEX,
    CONF_RESOLUTION,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from . import DallasComponent, dallas_ns

DallasTemperatureSensor = dallas_ns.class_("DallasTemperatureSensor", sensor.Sensor)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        DallasTemperatureSensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(
        {
            cv.GenerateID(CONF_DALLAS_ID): cv.use_id(DallasComponent),
            cv.Optional(CONF_ADDRESS): cv.hex_uint64_t,
            cv.Optional(CONF_INDEX): cv.positive_int,
            cv.Optional(CONF_RESOLUTION, default=12): cv.int_range(min=9, max=12),
        }
    ),
    cv.has_exactly_one_key(CONF_ADDRESS, CONF_INDEX),
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DALLAS_ID])
    var = await sensor.new_sensor(config)

    if CONF_ADDRESS in config:
        cg.add(var.set_address(config[CONF_ADDRESS]))
    else:
        cg.add(var.set_index(config[CONF_INDEX]))

    if CONF_RESOLUTION in config:
        cg.add(var.set_resolution(config[CONF_RESOLUTION]))

    cg.add(var.set_parent(hub))

    cg.add(hub.register_sensor(var))
