import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import (
    CONF_ADDRESSABLE_LIGHT_ID,
    CONF_FROM,
    CONF_ID,
    CONF_LIGHT_ID,
    CONF_SEGMENTS,
    CONF_SINGLE_LIGHT_ID,
    CONF_TO,
    CONF_OUTPUT_ID,
    CONF_REVERSED,
)

partitions_ns = cg.esphome_ns.namespace("partition")
AddressableSegment = partitions_ns.class_("AddressableSegment")
AddressableLightWrapper = cg.esphome_ns.namespace("light").class_(
    "AddressableLightWrapper"
)
PartitionLightOutput = partitions_ns.class_(
    "PartitionLightOutput", light.AddressableLight
)


def validate_from_to(value):
    if CONF_ID in value and value[CONF_FROM] > value[CONF_TO]:
        raise cv.Invalid(
            f"From ({value[CONF_FROM]}) must not be larger than to ({value[CONF_TO]})"
        )
    return value


ADDRESSABLE_SEGMENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(light.AddressableLightState),
        cv.Required(CONF_FROM): cv.positive_int,
        cv.Required(CONF_TO): cv.positive_int,
        cv.Optional(CONF_REVERSED, default=False): cv.boolean,
    }
)

NONADDRESSABLE_SEGMENT_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.Required(CONF_SINGLE_LIGHT_ID): cv.use_id(light.LightState),
        cv.GenerateID(CONF_ADDRESSABLE_LIGHT_ID): cv.declare_id(
            AddressableLightWrapper
        ),
        cv.GenerateID(CONF_LIGHT_ID): cv.declare_id(light.types.LightState),
    }
)

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(PartitionLightOutput),
        cv.Required(CONF_SEGMENTS): cv.All(
            cv.ensure_list(
                cv.Any(ADDRESSABLE_SEGMENT_SCHEMA, NONADDRESSABLE_SEGMENT_SCHEMA),
                validate_from_to,
            ),
            cv.Length(min=1),
        ),
    }
)


async def to_code(config):
    segments = []
    for conf in config[CONF_SEGMENTS]:
        if CONF_SINGLE_LIGHT_ID in conf:
            wrapper = cg.new_Pvariable(
                conf[CONF_ADDRESSABLE_LIGHT_ID],
                await cg.get_variable(conf[CONF_SINGLE_LIGHT_ID]),
            )
            light_state = cg.new_Pvariable(conf[CONF_LIGHT_ID], "", wrapper)
            await cg.register_component(light_state, conf)
            cg.add(cg.App.register_light(light_state))
            segments.append(AddressableSegment(light_state, 0, 1, False))

        else:
            segments.append(
                AddressableSegment(
                    await cg.get_variable(conf[CONF_ID]),
                    conf[CONF_FROM],
                    conf[CONF_TO] - conf[CONF_FROM] + 1,
                    conf[CONF_REVERSED],
                )
            )

    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], segments)
    await cg.register_component(var, config)
    await light.register_light(var, config)
