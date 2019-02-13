import voluptuous as vol

from esphome.components import fan, mqtt, output
import esphome.config_validation as cv
from esphome.const import CONF_HIGH, CONF_LOW, CONF_MAKE_ID, CONF_MEDIUM, CONF_NAME, \
    CONF_OSCILLATION_OUTPUT, CONF_OUTPUT, CONF_SPEED, CONF_SPEED_COMMAND_TOPIC, \
    CONF_SPEED_STATE_TOPIC
from esphome.cpp_generator import add, get_variable, variable
from esphome.cpp_types import App

PLATFORM_SCHEMA = cv.nameable(fan.FAN_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(fan.MakeFan),
    vol.Required(CONF_OUTPUT): cv.use_variable_id(output.FloatOutput),
    vol.Optional(CONF_SPEED_STATE_TOPIC): cv.publish_topic,
    vol.Optional(CONF_SPEED_COMMAND_TOPIC): cv.subscribe_topic,
    vol.Optional(CONF_OSCILLATION_OUTPUT): cv.use_variable_id(output.BinaryOutput),
    vol.Optional(CONF_SPEED): vol.Schema({
        vol.Required(CONF_LOW): cv.percentage,
        vol.Required(CONF_MEDIUM): cv.percentage,
        vol.Required(CONF_HIGH): cv.percentage,
    }),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for output_ in get_variable(config[CONF_OUTPUT]):
        yield
    rhs = App.make_fan(config[CONF_NAME])
    fan_struct = variable(config[CONF_MAKE_ID], rhs)
    if CONF_SPEED in config:
        speeds = config[CONF_SPEED]
        add(fan_struct.Poutput.set_speed(output_,
                                         speeds[CONF_LOW],
                                         speeds[CONF_MEDIUM],
                                         speeds[CONF_HIGH]))
    else:
        add(fan_struct.Poutput.set_speed(output_))

    if CONF_OSCILLATION_OUTPUT in config:
        for oscillation_output in get_variable(config[CONF_OSCILLATION_OUTPUT]):
            yield
        add(fan_struct.Poutput.set_oscillation(oscillation_output))

    fan.setup_fan(fan_struct.Pstate, config)


def to_hass_config(data, config):
    ret = fan.core_to_hass_config(data, config)
    if ret is None:
        return None
    default = mqtt.get_default_topic_for(data, 'fan', config[CONF_NAME], 'speed/state')
    ret['speed_state_topic'] = config.get(CONF_SPEED_STATE_TOPIC, default)
    default = mqtt.get_default_topic_for(data, 'fan', config[CONF_NAME], 'speed/command')
    ret['speed_command__topic'] = config.get(CONF_SPEED_COMMAND_TOPIC, default)
    return ret
