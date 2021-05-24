import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import (
    CONF_FROM,
    CONF_ID,
    CONF_SEGMENTS,
    CONF_TO,
    CONF_OUTPUT_ID,
    CONF_REVERSED,
)

partitions_ns = cg.esphome_ns.namespace("partition")
AddressableSegment = partitions_ns.class_("AddressableSegment")
PartitionLightOutput = partitions_ns.class_(
    "PartitionLightOutput", light.AddressableLight
)


def validate_from_to(value):
    if value[CONF_FROM] > value[CONF_TO]:
        raise cv.Invalid(
            "From ({}) must not be larger than to ({})"
            "".format(value[CONF_FROM], value[CONF_TO])
        )
    return value


CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(PartitionLightOutput),
        cv.Required(CONF_SEGMENTS): cv.All(
            cv.ensure_list(
                {
                    cv.Required(CONF_ID): cv.use_id(light.AddressableLightState),
                    cv.Required(CONF_FROM): cv.positive_int,
                    cv.Required(CONF_TO): cv.positive_int,
                    cv.Optional(CONF_REVERSED, default=False): cv.boolean,
                },
                validate_from_to,
            ),
            cv.Length(min=1),
        ),
    }
)


async def to_code(config):
    segments = []
    for conf in config[CONF_SEGMENTS]:
        var = await cg.get_variable(conf[CONF_ID])
        segments.append(
            AddressableSegment(
                var,
                conf[CONF_FROM],
                conf[CONF_TO] - conf[CONF_FROM] + 1,
                conf[CONF_REVERSED],
            )
        )

    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], segments)
    await cg.register_component(var, config)
    await light.register_light(var, config)
