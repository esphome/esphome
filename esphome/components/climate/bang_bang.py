import voluptuous as vol

from esphome import automation
from esphome.components import climate, sensor
import esphome.config_validation as cv
from esphome.const import CONF_AWAY_CONFIG, CONF_COOL_ACTION, \
    CONF_DEFAULT_TARGET_TEMPERATURE_HIGH, \
    CONF_DEFAULT_TARGET_TEMPERATURE_LOW, CONF_HEAT_ACTION, CONF_ID, CONF_IDLE_ACTION, CONF_NAME, \
    CONF_SENSOR
from esphome.cpp_generator import Pvariable, add, get_variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App

BangBangClimate = climate.climate_ns.class_('BangBangClimate', climate.ClimateDevice)
BangBangClimateTargetTempConfig = climate.climate_ns.struct('BangBangClimateTargetTempConfig')

PLATFORM_SCHEMA = cv.nameable(climate.CLIMATE_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(BangBangClimate),
    vol.Required(CONF_SENSOR): cv.use_variable_id(sensor.Sensor),
    vol.Required(CONF_DEFAULT_TARGET_TEMPERATURE_LOW): cv.temperature,
    vol.Required(CONF_DEFAULT_TARGET_TEMPERATURE_HIGH): cv.temperature,
    vol.Required(CONF_IDLE_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_COOL_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_HEAT_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_AWAY_CONFIG): cv.Schema({
        vol.Required(CONF_DEFAULT_TARGET_TEMPERATURE_LOW): cv.temperature,
        vol.Required(CONF_DEFAULT_TARGET_TEMPERATURE_HIGH): cv.temperature,
    }),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.register_component(BangBangClimate.new(config[CONF_NAME]))
    control = Pvariable(config[CONF_ID], rhs)
    climate.register_climate(control, config)
    setup_component(control, config)

    for var in get_variable(config[CONF_SENSOR]):
        yield
    add(control.set_sensor(var))

    normal_config = BangBangClimateTargetTempConfig(
        config[CONF_DEFAULT_TARGET_TEMPERATURE_LOW],
        config[CONF_DEFAULT_TARGET_TEMPERATURE_HIGH]
    )
    add(control.set_normal_config(normal_config))

    automation.build_automations(control.get_idle_trigger(), [], config[CONF_IDLE_ACTION])

    if CONF_COOL_ACTION in config:
        automation.build_automations(control.get_cool_trigger(), [], config[CONF_COOL_ACTION])
        add(control.set_supports_cool(True))
    if CONF_HEAT_ACTION in config:
        automation.build_automations(control.get_heat_trigger(), [], config[CONF_HEAT_ACTION])
        add(control.set_supports_heat(True))

    if CONF_AWAY_CONFIG in config:
        away = config[CONF_AWAY_CONFIG]
        away_config = BangBangClimateTargetTempConfig(
            away[CONF_DEFAULT_TARGET_TEMPERATURE_LOW],
            away[CONF_DEFAULT_TARGET_TEMPERATURE_HIGH]
        )
        add(control.set_away_config(away_config))


BUILD_FLAGS = '-DUSE_BANG_BANG_CLIMATE'
