import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    CONF_ID,
)
from . import DS3231Component, ds3231_ns, CONF_DS3231_ID


DEPENDENCIES = ["ds3231"]

DS3231Sensor = ds3231_ns.class_("DS3231Sensor", cg.PollingComponent, sensor.Sensor)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(DS3231Sensor),
            cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    ds3231 = await cg.get_variable(config[CONF_DS3231_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, ds3231)
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
