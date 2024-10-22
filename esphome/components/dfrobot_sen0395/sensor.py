import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.components.sensor import (
    validate_device_class,
    validate_unit_of_measurement,
)
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_TARGET,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_DISTANCE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
)

from . import CONF_DFROBOT_SEN0395_ID, DfrobotSen0395Component

DEPENDENCIES = ["dfrobot_sen0395"]

_SENSOR_SCHEMA = (
    sensor.sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=2,
    )
    .extend(
        {
            cv.GenerateID(CONF_DFROBOT_SEN0395_ID): cv.use_id(DfrobotSen0395Component),
            cv.Required(CONF_TARGET): cv.int_range(min=1, max=9),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        "distance": _SENSOR_SCHEMA.extend(
            {
                cv.Optional(
                    CONF_UNIT_OF_MEASUREMENT, default="m"
                ): validate_unit_of_measurement,
                cv.Optional(
                    CONF_DEVICE_CLASS, default=DEVICE_CLASS_DISTANCE
                ): validate_device_class,
            },
        ),
        "SNR": _SENSOR_SCHEMA.extend({}),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DFROBOT_SEN0395_ID])
    binary_sens = await sensor.new_sensor(config)

    if config[CONF_TYPE] == "distance":
        cg.add(
            parent.set_detected_target_distance_sensor(config[CONF_TARGET], binary_sens)
        )
    elif config[CONF_TYPE] == "SNR":
        cg.add(parent.set_detected_target_snr_sensor(config[CONF_TARGET], binary_sens))
