import logging

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from esphome.types import ConfigType

from .. import BEDJET_CLIENT_SCHEMA, bedjet_ns, register_bedjet_child

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@jhansche", "@javawizard"]
DEPENDENCIES = ["bedjet"]

CONF_OUTLET_TEMPERATURE = "outlet_temperature"
CONF_AMBIENT_TEMPERATURE = "ambient_temperature"

BedjetSensor = bedjet_ns.class_("BedjetSensor", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BedjetSensor),
        cv.Optional(CONF_OUTLET_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_AMBIENT_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(BEDJET_CLIENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_bedjet_child(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_OUTLET_TEMPERATURE, var.set_outlet_temperature_sensor)
    await sensors(CONF_AMBIENT_TEMPERATURE, var.set_ambient_temperature_sensor)
