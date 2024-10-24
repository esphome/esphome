import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_DALLAS_ID,
    CONF_INDEX,
    CONF_RESOLUTION,
    CONF_CHANNEL,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from .. import DS248xComponent, ds248x_ns

DS248xTemperatureSensor = ds248x_ns.class_("DS248xTemperatureSensor", sensor.Sensor)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        DS248xTemperatureSensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(
        {
            cv.GenerateID(CONF_DALLAS_ID): cv.use_id(DS248xComponent),
            cv.Optional(CONF_ADDRESS): cv.hex_int,
            cv.Optional(CONF_INDEX): cv.positive_int,
            cv.Optional(CONF_RESOLUTION, default=12): cv.int_range(min=9, max=12),
            cv.Optional(CONF_CHANNEL, default=0): cv.int_range(min=0, max=7),
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

    if CONF_CHANNEL in config:
        cg.add(var.set_channel(config[CONF_CHANNEL]))

    cg.add(var.set_parent(hub))

    cg.add(hub.register_sensor(var))
