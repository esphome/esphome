import voluptuous as vol

from esphome.components import light
from esphome.components.light import AddressableLight
import esphome.config_validation as cv
from esphome.const import CONF_DEFAULT_TRANSITION_LENGTH, CONF_EFFECTS, CONF_FROM, CONF_ID, \
    CONF_MAKE_ID, CONF_NAME, CONF_SEGMENTS, CONF_TO
from esphome.cpp_generator import get_variable, variable
from esphome.cpp_types import App, Application

AddressableSegment = light.light_ns.class_('AddressableSegment')
PartitionLightOutput = light.light_ns.class_('PartitionLightOutput', AddressableLight)
MakePartitionLight = Application.struct('MakePartitionLight')


def validate_from_to(value):
    if value[CONF_FROM] > value[CONF_TO]:
        raise vol.Invalid(u"From ({}) must not be larger than to ({})"
                          u"".format(value[CONF_FROM], value[CONF_TO]))
    return value


PLATFORM_SCHEMA = cv.nameable(light.LIGHT_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(light.AddressableLightState),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakePartitionLight),

    vol.Required(CONF_SEGMENTS): vol.All(cv.ensure_list({
        vol.Required(CONF_ID): cv.use_variable_id(light.AddressableLightState),
        vol.Required(CONF_FROM): cv.positive_int,
        vol.Required(CONF_TO): cv.positive_int,
    }, validate_from_to), vol.Length(min=1)),

    vol.Optional(CONF_DEFAULT_TRANSITION_LENGTH): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_EFFECTS): light.validate_effects(light.ADDRESSABLE_EFFECTS),
}))


def to_code(config):
    segments = []
    for conf in config[CONF_SEGMENTS]:
        for var in get_variable(conf[CONF_ID]):
            yield
        segments.append(AddressableSegment(var, conf[CONF_FROM],
                                           conf[CONF_TO] - conf[CONF_FROM] + 1))

    rhs = App.make_partition_light(config[CONF_NAME], segments)
    make = variable(config[CONF_MAKE_ID], rhs)
    light.setup_light(make.Pstate, config)


def to_hass_config(data, config):
    return light.core_to_hass_config(data, config, brightness=True, rgb=True, color_temp=False)
