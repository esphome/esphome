import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.const import CONF_FACTORY_RESET, CONF_ID, CONF_SENSITIVITY
from esphome.components import uart

CODEOWNERS = ["@niklasweber"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

dfrobot_sen0395_ns = cg.esphome_ns.namespace("dfrobot_sen0395")
DfrobotSen0395Component = dfrobot_sen0395_ns.class_(
    "DfrobotSen0395Component", cg.Component
)

# Actions
DfrobotSen0395ResetAction = dfrobot_sen0395_ns.class_(
    "DfrobotSen0395ResetAction", automation.Action
)
DfrobotSen0395SettingsAction = dfrobot_sen0395_ns.class_(
    "DfrobotSen0395SettingsAction", automation.Action
)

CONF_DFROBOT_SEN0395_ID = "dfrobot_sen0395_id"

CONF_DELAY_AFTER_DETECT = "delay_after_detect"
CONF_DELAY_AFTER_DISAPPEAR = "delay_after_disappear"
CONF_DETECTION_SEGMENTS = "detection_segments"
CONF_OUTPUT_LATENCY = "output_latency"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DfrobotSen0395Component),
        }
    ).extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)


@automation.register_action(
    "dfrobot_sen0395.reset",
    DfrobotSen0395ResetAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DfrobotSen0395Component),
        }
    ),
)
async def dfrobot_sen0395_reset_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    return var


def range_segment_list(input):
    """Validate input is a list of ranges which can be used to configure the dfrobot mmwave radar

    A list of segments should be provided. A minimum of one segment is required and a maximum of
    four segments is allowed. A segment describes a range of distances. E.g. from 0mm to 1m.
    The distances need to be defined in an ascending order and they cannot contain / intersect
    each other.
    """

    # Flatten input to one dimensional list
    flat_list = []
    if isinstance(input, list):
        for list_item in input:
            if isinstance(list_item, list):
                for item in list_item:
                    flat_list.append(item)
            else:
                flat_list.append(list_item)
    else:
        flat_list.append(input)

    input = flat_list

    if len(input) < 2:
        raise cv.Invalid(
            "At least two values need to be specified (start + stop distances)"
        )
    if len(input) % 2 != 0:
        raise cv.Invalid(
            "An even number of arguments must be specified (pairs of min + max)"
        )
    if len(input) > 8:
        raise cv.Invalid(
            "Maximum four segments can be specified (8 values: 4 * min + max)"
        )

    largest_distance = -1
    for distance in input:
        if isinstance(distance, cv.Lambda):
            continue
        m = cv.distance(distance)
        if m > 9:
            raise cv.Invalid("Maximum distance is 9m")
        if m < 0:
            raise cv.Invalid("Minimum distance is 0m")
        if m <= largest_distance:
            raise cv.Invalid(
                "Distances must be delared from small to large "
                "and they cannot contain each other"
            )
        largest_distance = m
        # Replace distance object with meters float
        input[input.index(distance)] = m

    return input


MMWAVE_SETTINGS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DfrobotSen0395Component),
        cv.Optional(CONF_FACTORY_RESET): cv.templatable(cv.boolean),
        cv.Optional(CONF_DETECTION_SEGMENTS): range_segment_list,
        cv.Optional(CONF_OUTPUT_LATENCY): {
            cv.Required(CONF_DELAY_AFTER_DETECT): cv.templatable(
                cv.All(
                    cv.positive_time_period_milliseconds,
                    cv.Range(max=cv.TimePeriod(seconds=1638.375)),
                )
            ),
            cv.Required(CONF_DELAY_AFTER_DISAPPEAR): cv.templatable(
                cv.All(
                    cv.positive_time_period_milliseconds,
                    cv.Range(max=cv.TimePeriod(seconds=1638.375)),
                )
            ),
        },
        cv.Optional(CONF_SENSITIVITY): cv.templatable(cv.int_range(min=0, max=9)),
    }
).add_extra(
    cv.has_at_least_one_key(
        CONF_FACTORY_RESET,
        CONF_DETECTION_SEGMENTS,
        CONF_OUTPUT_LATENCY,
        CONF_SENSITIVITY,
    )
)


@automation.register_action(
    "dfrobot_sen0395.settings",
    DfrobotSen0395SettingsAction,
    MMWAVE_SETTINGS_SCHEMA,
)
async def dfrobot_sen0395_settings_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    if factory_reset_config := config.get(CONF_FACTORY_RESET):
        template_ = await cg.templatable(factory_reset_config, args, int)
        cg.add(var.set_factory_reset(template_))

    if CONF_DETECTION_SEGMENTS in config:
        segments = config[CONF_DETECTION_SEGMENTS]

        if len(segments) >= 2:
            template_ = await cg.templatable(segments[0], args, float)
            cg.add(var.set_det_min1(template_))
            template_ = await cg.templatable(segments[1], args, float)
            cg.add(var.set_det_max1(template_))
        if len(segments) >= 4:
            template_ = await cg.templatable(segments[2], args, float)
            cg.add(var.set_det_min2(template_))
            template_ = await cg.templatable(segments[3], args, float)
            cg.add(var.set_det_max2(template_))
        if len(segments) >= 6:
            template_ = await cg.templatable(segments[4], args, float)
            cg.add(var.set_det_min3(template_))
            template_ = await cg.templatable(segments[5], args, float)
            cg.add(var.set_det_max3(template_))
        if len(segments) >= 8:
            template_ = await cg.templatable(segments[6], args, float)
            cg.add(var.set_det_min4(template_))
            template_ = await cg.templatable(segments[7], args, float)
            cg.add(var.set_det_max4(template_))
    if CONF_OUTPUT_LATENCY in config:
        template_ = await cg.templatable(
            config[CONF_OUTPUT_LATENCY][CONF_DELAY_AFTER_DETECT], args, float
        )
        if isinstance(template_, cv.TimePeriod):
            template_ = template_.total_milliseconds / 1000
        cg.add(var.set_delay_after_detect(template_))

        template_ = await cg.templatable(
            config[CONF_OUTPUT_LATENCY][CONF_DELAY_AFTER_DISAPPEAR], args, float
        )
        if isinstance(template_, cv.TimePeriod):
            template_ = template_.total_milliseconds / 1000
        cg.add(var.set_delay_after_disappear(template_))
    if CONF_SENSITIVITY in config:
        template_ = await cg.templatable(config[CONF_SENSITIVITY], args, int)
        cg.add(var.set_sensitivity(template_))

    return var
