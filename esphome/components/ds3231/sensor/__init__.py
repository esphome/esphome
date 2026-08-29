import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_TEMPERATURE,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from esphome.types import ConfigType

from .. import CONF_DS3231_ID, DS3231Component, ds3231_ns

DEPENDENCIES = ["ds3231"]

CONF_UNIT = "unit"
UNIT_FAHRENHEIT = "°F"

UNITS = {
    "celsius": UNIT_CELSIUS,
    "fahrenheit": UNIT_FAHRENHEIT,
}

DS3231TemperatureSensor = ds3231_ns.class_(
    "DS3231TemperatureSensor",
    sensor.Sensor,
    cg.PollingComponent,
    cg.Parented.template(DS3231Component),
)


def _default_unit_of_measurement(config: ConfigType) -> ConfigType:
    # The chip reports Celsius; when the user picks Fahrenheit (and hasn't set the
    # displayed unit explicitly) default unit_of_measurement to match.
    if CONF_UNIT_OF_MEASUREMENT not in config:
        config[CONF_UNIT_OF_MEASUREMENT] = UNITS[config[CONF_UNIT]]
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        DS3231TemperatureSensor,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        icon=ICON_THERMOMETER,
    )
    .extend(
        {
            cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component),
            cv.Optional(CONF_UNIT, default="celsius"): cv.one_of(*UNITS, lower=True),
        }
    )
    .extend(cv.polling_component_schema("60s")),
    _default_unit_of_measurement,
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_DS3231_ID])
    cg.add(var.set_fahrenheit(config[CONF_UNIT] == "fahrenheit"))
