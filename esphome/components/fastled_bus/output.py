import copy
import esphome.config_validation as cv
from esphome.components import output
import esphome.codegen as cg
from esphome.const import CONF_CHANNELS, CONF_ID, CONF_NUM_CHIPS, CONF_OFFSET
from . import CONF_BUS, CONF_REPEAT_DISTANCE, CONF_CHANNEL_OFFSET, bus_ns

# MULTI_CONF = True

clazz_output = bus_ns.class_("Output", output.FloatOutput, cg.Component)

bus_ns_channels = bus_ns.class_("FastledBusChannels")
Mapping = bus_ns.struct("Mapping")
MappingsBuilder_ns = bus_ns.namespace("MappingsBuilder")
MappingsBuilder = bus_ns.class_("MappingsBuilder")

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(clazz_output),
        cv.Required(CONF_CHANNELS): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_BUS): cv.use_id(CONF_BUS),
                    cv.Required(CONF_OFFSET): cv.positive_int,
                    cv.Required(CONF_NUM_CHIPS): cv.positive_not_null_int,
                    cv.Required(CONF_CHANNEL_OFFSET): cv.positive_int,
                    cv.Optional(CONF_REPEAT_DISTANCE): cv.positive_not_null_int,
                }
            )
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    channels = list(config[CONF_CHANNELS])
    template_len = cg.TemplateArguments(cg.RawExpression(f"{len(channels)}"))
    out = bus_ns.mappings_builder_create(template_len)

    for channel in channels:
        bus = await cg.get_variable(channel[CONF_BUS])
        crd = 0
        if CONF_REPEAT_DISTANCE in channel:
            crd = channel[CONF_REPEAT_DISTANCE]
        out = out.add_mapping(
            bus,
            channel[CONF_NUM_CHIPS],
            channel[CONF_OFFSET],
            channel[CONF_CHANNEL_OFFSET],
            crd,
        )
        # out.append(
        #     cg.StructInitializer(
        #         Mapping,
        #         ("bus_", bus),
        #         ("num_chips_", channel[CONF_OFFSET]),
        #         ("ofs_", channel[CONF_NUM_CHIPS]),
        #         ("channel_offset_", channel[CONF_CHANNEL_OFFSET]),
        #         ("repeat_distance_", crd),
        #     )
        # )
    # cg.add(
    #     cg.AssignmentExpression(
    #         Mapping,
    #         "",
    #         f"_{config[CONF_ID]}[{len(out)}]",
    #         cg.ArrayInitializer(*out, multiline=True),
    #     )
    # )

    # very ugly things to add template parameters to type declaration.

    config = config.copy()
    config[CONF_ID] = copy.deepcopy(config[CONF_ID])
    config[CONF_ID].type.base = f"{config[CONF_ID].type.base}<{len(channels)}>"
    var = cg.new_Pvariable(
        # config[CONF_ID], cg.ArrayInitializer(*out, multiline=True, member_type=Mapping)
        config[CONF_ID],
        out.done(),
    )

    # var = cg.new_Pvariable(
    #     config[CONF_ID], cg.RawExpression(f"_{config[CONF_ID]}")
    # )
    # out = f"({Mapping}[{len(channels)}]){{\n"
    # comma = ""
    # for channel in channels:
    #     bus = await cg.get_variable(channel[CONF_BUS])
    #     crd = 0
    #     if CONF_REPEAT_DISTANCE in channel:
    #         crd = channel[CONF_REPEAT_DISTANCE]
    #     out += f'{comma}{{ {bus}, {channel[CONF_NUM_CHIPS]}, {channel[CONF_OFFSET]}, {channel[CONF_CHANNEL_OFFSET]}, {crd} }}'
    #     comma = ",\n"
    # out += "\n}"
    # var = cg.new_Pvariable(config[CONF_ID], len(out), cg.RawExpression(out))
    await cg.register_component(var, config)
    await output.register_output(var, config)
