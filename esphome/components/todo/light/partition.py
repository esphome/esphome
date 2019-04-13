from esphome.components import light
from esphome.components.light import AddressableLight
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_FROM, CONF_ID, CONF_MAKE_ID, CONF_NAME, CONF_SEGMENTS, CONF_TO
AddressableSegment = light.light_ns.class_('AddressableSegment')
PartitionLightOutput = light.light_ns.class_('PartitionLightOutput', AddressableLight)
MakePartitionLight = Application.struct('MakePartitionLight')


def validate_from_to(value):
    if value[CONF_FROM] > value[CONF_TO]:
        raise cv.Invalid(u"From ({}) must not be larger than to ({})"
                          u"".format(value[CONF_FROM], value[CONF_TO]))
    return value


PLATFORM_SCHEMA = cv.nameable(light.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(light.AddressableLightState),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakePartitionLight),

    cv.Required(CONF_SEGMENTS): cv.All(cv.ensure_list({
        cv.Required(CONF_ID): cv.use_variable_id(light.AddressableLightState),
        cv.Required(CONF_FROM): cv.positive_int,
        cv.Required(CONF_TO): cv.positive_int,
    }, validate_from_to), cv.Length(min=1)),
}).extend(light.ADDRESSABLE_LIGHT_SCHEMA.schema))


def to_code(config):
    segments = []
    for conf in config[CONF_SEGMENTS]:
        var = yield get_variable(conf[CONF_ID])
        segments.append(AddressableSegment(var, conf[CONF_FROM],
                                           conf[CONF_TO] - conf[CONF_FROM] + 1))

    rhs = App.make_partition_light(config[CONF_NAME], segments)
    make = variable(config[CONF_MAKE_ID], rhs)
    light.setup_light(make.Pstate, make.Ppartition, config)
