"""Compatibility sensor-platform configuration for VL6180X."""

import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.vl6180x import (
    ALS_GAINS,
    CONF_GAIN,
    VL6180XComponent,
    _request_instance_slot,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_BRIGHTNESS_5,
    ICON_RULER,
    UNIT_LUX,
    UNIT_MILLIMETER,
)
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["text_sensor"]

CONF_DISTANCE = "distance"
CONF_ALS = "als"

ALS_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_LUX,
    icon=ICON_BRIGHTNESS_5,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_ILLUMINANCE,
).extend({cv.Optional(CONF_GAIN, default=1): cv.enum(ALS_GAINS)})

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VL6180XComponent),
            cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIMETER,
                icon=ICON_RULER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DISTANCE,
            ),
            cv.Optional(CONF_ALS): ALS_SENSOR_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x29)),
    cv.has_at_least_one_key(CONF_DISTANCE, CONF_ALS),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_name_str(config[CONF_ID].id))
    _request_instance_slot()
    cg.add(var.register_instance(var))

    if distance_config := config.get(CONF_DISTANCE):
        distance_sensor = await sensor.new_sensor(distance_config)
        cg.add(var.set_range_sensor(distance_sensor))

    if als_config := config.get(CONF_ALS):
        lux_sensor = await sensor.new_sensor(als_config)
        cg.add(var.set_lux_sensor(lux_sensor))
        cg.add(var.set_als_gain(als_config[CONF_GAIN]))
