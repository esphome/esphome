import voluptuous as vol

from esphome import core
from esphome.automation import ACTION_REGISTRY
from esphome.components import mqtt
from esphome.components.mqtt import setup_mqtt_component
import esphome.config_validation as cv
from esphome.const import CONF_AWAY, CONF_ID, CONF_INTERNAL, CONF_MAX_TEMPERATURE, \
    CONF_MIN_TEMPERATURE, CONF_MODE, CONF_MQTT_ID, CONF_TARGET_TEMPERATURE, \
    CONF_TARGET_TEMPERATURE_HIGH, CONF_TARGET_TEMPERATURE_LOW, CONF_TEMPERATURE_STEP, CONF_VISUAL
from esphome.core import CORE
from esphome.cpp_generator import Pvariable, add, get_variable, templatable
from esphome.cpp_types import Action, App, Nameable, bool_, esphome_ns, float_

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

climate_ns = esphome_ns.namespace('climate')

ClimateDevice = climate_ns.class_('ClimateDevice', Nameable)
ClimateCall = climate_ns.class_('ClimateCall')
ClimateTraits = climate_ns.class_('ClimateTraits')
MQTTClimateComponent = climate_ns.class_('MQTTClimateComponent', mqtt.MQTTComponent)

ClimateMode = climate_ns.enum('ClimateMode')
CLIMATE_MODES = {
    'OFF': ClimateMode.CLIMATE_MODE_OFF,
    'AUTO': ClimateMode.CLIMATE_MODE_AUTO,
    'COOL': ClimateMode.CLIMATE_MODE_COOL,
    'HEAT': ClimateMode.CLIMATE_MODE_HEAT,
}

validate_climate_mode = cv.one_of(*CLIMATE_MODES, upper=True)

# Actions
ControlAction = climate_ns.class_('ControlAction', Action)

CLIMATE_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(ClimateDevice),
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTClimateComponent),
    vol.Optional(CONF_VISUAL, default={}): cv.Schema({
        vol.Optional(CONF_MIN_TEMPERATURE): cv.temperature,
        vol.Optional(CONF_MAX_TEMPERATURE): cv.temperature,
        vol.Optional(CONF_TEMPERATURE_STEP): cv.temperature,
    })
})

CLIMATE_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(CLIMATE_SCHEMA.schema)


def setup_climate_core_(climate_var, config):
    if CONF_INTERNAL in config:
        add(climate_var.set_internal(config[CONF_INTERNAL]))
    visual = config[CONF_VISUAL]
    if CONF_MIN_TEMPERATURE in visual:
        add(climate_var.set_visual_min_temperature_override(visual[CONF_MIN_TEMPERATURE]))
    if CONF_MAX_TEMPERATURE in visual:
        add(climate_var.set_visual_max_temperature_override(visual[CONF_MAX_TEMPERATURE]))
    if CONF_TEMPERATURE_STEP in visual:
        add(climate_var.set_visual_temperature_step_override(visual[CONF_TEMPERATURE_STEP]))
    setup_mqtt_component(climate_var.Pget_mqtt(), config)


def register_climate(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = Pvariable(config[CONF_ID], var, has_side_effects=True)
    add(App.register_climate(var))
    CORE.add_job(setup_climate_core_, var, config)


BUILD_FLAGS = '-DUSE_CLIMATE'

CONF_CLIMATE_CONTROL = 'climate.control'
CLIMATE_CONTROL_ACTION_SCHEMA = cv.Schema({
    vol.Required(CONF_ID): cv.use_variable_id(ClimateDevice),
    vol.Optional(CONF_MODE): cv.templatable(validate_climate_mode),
    vol.Optional(CONF_TARGET_TEMPERATURE): cv.templatable(cv.temperature),
    vol.Optional(CONF_TARGET_TEMPERATURE_LOW): cv.templatable(cv.temperature),
    vol.Optional(CONF_TARGET_TEMPERATURE_HIGH): cv.templatable(cv.temperature),
    vol.Optional(CONF_AWAY): cv.templatable(cv.boolean),
})


@ACTION_REGISTRY.register(CONF_CLIMATE_CONTROL, CLIMATE_CONTROL_ACTION_SCHEMA)
def climate_control_to_code(config, action_id, template_arg, args):
    for var in get_variable(config[CONF_ID]):
        yield None
    type = ControlAction.template(template_arg)
    rhs = type.new(var)
    action = Pvariable(action_id, rhs, type=type)
    if CONF_MODE in config:
        if isinstance(config[CONF_MODE], core.Lambda):
            for template_ in templatable(config[CONF_MODE], args, ClimateMode):
                yield None
            add(action.set_mode(template_))
        else:
            add(action.set_mode(CLIMATE_MODES[config[CONF_MODE]]))
    if CONF_TARGET_TEMPERATURE in config:
        for template_ in templatable(config[CONF_TARGET_TEMPERATURE], args, float_):
            yield None
        add(action.set_target_temperature(template_))
    if CONF_TARGET_TEMPERATURE_LOW in config:
        for template_ in templatable(config[CONF_TARGET_TEMPERATURE_LOW], args, float_):
            yield None
        add(action.set_target_temperature_low(template_))
    if CONF_TARGET_TEMPERATURE_HIGH in config:
        for template_ in templatable(config[CONF_TARGET_TEMPERATURE_HIGH], args, float_):
            yield None
        add(action.set_target_temperature_high(template_))
    if CONF_AWAY in config:
        for template_ in templatable(config[CONF_AWAY], args, bool_):
            yield None
        add(action.set_away(template_))
    yield action
