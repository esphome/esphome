import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
    CONF_EQUATION,
    ICON_WATER,
    UNIT_GRAMS_PER_CUBIC_METER,
)

absolute_humidity_ns = cg.esphome_ns.namespace("absolute_humidity")
AbsoluteHumidityComponent = absolute_humidity_ns.class_(
    "AbsoluteHumidityComponent", sensor.Sensor, cg.Component
)

SaturatedVaporPressureEquation = absolute_humidity_ns.enum(
    "SaturatedVaporPressureEquation"
)
EQUATION = {
    "BUCK": SaturatedVaporPressureEquation.BUCK,
    "TETENS": SaturatedVaporPressureEquation.TETENS,
    "WOBUS": SaturatedVaporPressureEquation.WOBUS,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_GRAMS_PER_CUBIC_METER,
        icon=ICON_WATER,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_EMPTY,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(AbsoluteHumidityComponent),
            cv.Required(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
            cv.Required(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_EQUATION, default="WOBUS"): cv.enum(EQUATION, upper=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    temperature_sensor = await cg.get_variable(config[CONF_TEMPERATURE])
    cg.add(var.set_temperature_sensor(temperature_sensor))

    humidity_sensor = await cg.get_variable(config[CONF_HUMIDITY])
    cg.add(var.set_humidity_sensor(humidity_sensor))

    cg.add(var.set_equation(config[CONF_EQUATION]))
