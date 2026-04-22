import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_PM_2_5,
    CONF_PM_10_0,
    DEVICE_CLASS_AQI,
    STATE_CLASS_MEASUREMENT,
)

from . import AQI_CALCULATION_TYPE, CONF_CALCULATION_TYPE, aqi_ns

CODEOWNERS = ["@jasstrong"]
DEPENDENCIES = ["sensor"]

AQISensor = aqi_ns.class_("AQISensor", sensor.Sensor, cg.Component)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        AQISensor,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_AQI,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_PM_2_5): cv.use_id(sensor.Sensor),
            cv.Required(CONF_PM_10_0): cv.use_id(sensor.Sensor),
            cv.Required(CONF_CALCULATION_TYPE): cv.enum(
                AQI_CALCULATION_TYPE, upper=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    pm_2_5_sensor = await cg.get_variable(config[CONF_PM_2_5])
    cg.add(var.set_pm_2_5_sensor(pm_2_5_sensor))

    pm_10_0_sensor = await cg.get_variable(config[CONF_PM_10_0])
    cg.add(var.set_pm_10_0_sensor(pm_10_0_sensor))

    cg.add(var.set_aqi_calculation_type(config[CONF_CALCULATION_TYPE]))
