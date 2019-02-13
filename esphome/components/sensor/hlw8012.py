import voluptuous as vol

from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_CF1_PIN, CONF_CF_PIN, CONF_CHANGE_MODE_EVERY, CONF_CURRENT, \
    CONF_CURRENT_RESISTOR, CONF_ID, CONF_NAME, CONF_POWER, CONF_SEL_PIN, CONF_UPDATE_INTERVAL, \
    CONF_VOLTAGE, CONF_VOLTAGE_DIVIDER
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_output_pin_expression, setup_component
from esphome.cpp_types import App, PollingComponent

HLW8012Component = sensor.sensor_ns.class_('HLW8012Component', PollingComponent)
HLW8012VoltageSensor = sensor.sensor_ns.class_('HLW8012VoltageSensor', sensor.EmptySensor)
HLW8012CurrentSensor = sensor.sensor_ns.class_('HLW8012CurrentSensor', sensor.EmptySensor)
HLW8012PowerSensor = sensor.sensor_ns.class_('HLW8012PowerSensor', sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HLW8012Component),
    vol.Required(CONF_SEL_PIN): pins.gpio_output_pin_schema,
    vol.Required(CONF_CF_PIN): pins.input_pin,
    vol.Required(CONF_CF1_PIN): pins.input_pin,

    vol.Optional(CONF_VOLTAGE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HLW8012VoltageSensor),
    })),
    vol.Optional(CONF_CURRENT): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HLW8012CurrentSensor),
    })),
    vol.Optional(CONF_POWER): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HLW8012PowerSensor),
    })),

    vol.Optional(CONF_CURRENT_RESISTOR): cv.resistance,
    vol.Optional(CONF_VOLTAGE_DIVIDER): cv.positive_float,
    vol.Optional(CONF_CHANGE_MODE_EVERY): vol.All(cv.uint32_t, vol.Range(min=1)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(CONF_VOLTAGE, CONF_CURRENT,
                                                               CONF_POWER))


def to_code(config):
    for sel in gpio_output_pin_expression(config[CONF_SEL_PIN]):
        yield

    rhs = App.make_hlw8012(sel, config[CONF_CF_PIN], config[CONF_CF1_PIN],
                           config.get(CONF_UPDATE_INTERVAL))
    hlw = Pvariable(config[CONF_ID], rhs)

    if CONF_VOLTAGE in config:
        conf = config[CONF_VOLTAGE]
        sensor.register_sensor(hlw.make_voltage_sensor(conf[CONF_NAME]), conf)
    if CONF_CURRENT in config:
        conf = config[CONF_CURRENT]
        sensor.register_sensor(hlw.make_current_sensor(conf[CONF_NAME]), conf)
    if CONF_POWER in config:
        conf = config[CONF_POWER]
        sensor.register_sensor(hlw.make_power_sensor(conf[CONF_NAME]), conf)
    if CONF_CURRENT_RESISTOR in config:
        add(hlw.set_current_resistor(config[CONF_CURRENT_RESISTOR]))
    if CONF_VOLTAGE_DIVIDER in config:
        add(hlw.set_voltage_divider(config[CONF_VOLTAGE_DIVIDER]))
    if CONF_CHANGE_MODE_EVERY in config:
        add(hlw.set_change_mode_every(config[CONF_CHANGE_MODE_EVERY]))
    setup_component(hlw, config)


BUILD_FLAGS = '-DUSE_HLW8012'


def to_hass_config(data, config):
    ret = []
    for key in (CONF_VOLTAGE, CONF_CURRENT, CONF_POWER):
        if key in config:
            ret.append(sensor.core_to_hass_config(data, config[key]))
    return ret
