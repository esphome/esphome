import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_WIND_SPEED,
    CONF_PIN,
    CONF_WIND_DIRECTION_DEGREES,
    DEVICE_CLASS_EMPTY,
    UNIT_KILOMETER_PER_HOUR,
    ICON_WEATHER_WINDY,
    ICON_SIGN_DIRECTION,
    UNIT_DEGREES,
)

tx20_ns = cg.esphome_ns.namespace("tx20")
Tx20Component = tx20_ns.class_("Tx20Component", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Tx20Component),
        cv.Optional(CONF_WIND_SPEED): sensor.sensor_schema(
            UNIT_KILOMETER_PER_HOUR, ICON_WEATHER_WINDY, 1, DEVICE_CLASS_EMPTY
        ),
        cv.Optional(CONF_WIND_DIRECTION_DEGREES): sensor.sensor_schema(
            UNIT_DEGREES, ICON_SIGN_DIRECTION, 1, DEVICE_CLASS_EMPTY
        ),
        cv.Required(CONF_PIN): cv.All(
            pins.internal_gpio_input_pin_schema, pins.validate_has_interrupt
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    if CONF_WIND_SPEED in config:
        conf = config[CONF_WIND_SPEED]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_wind_speed_sensor(sens))

    if CONF_WIND_DIRECTION_DEGREES in config:
        conf = config[CONF_WIND_DIRECTION_DEGREES]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_wind_direction_degrees_sensor(sens))

    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
