import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE as DEVICE_CLASS_TEMPERATURE,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT as STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS as UNIT_CELSIUS,
)
from esphome.types import ConfigType

from .. import CONF_DS3231_ID, DS3231Component, ds3231_ns

DEPENDENCIES = ["ds3231"]

DS3231TemperatureSensor = ds3231_ns.class_(
    "DS3231TemperatureSensor",
    sensor.Sensor,
    cg.PollingComponent,
    cg.Parented.template(DS3231Component),
)


CONFIG_SCHEMA = (
    sensor.sensor_schema(
        DS3231TemperatureSensor,
        kind=sensor.TEMPERATURE,
        accuracy_decimals=2,
        icon=ICON_THERMOMETER,
    )
    .extend({cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component)})
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_DS3231_ID])
