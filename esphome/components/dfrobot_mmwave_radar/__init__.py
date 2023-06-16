import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome import core
from esphome.automation import maybe_simple_id
from esphome.const import (
    CONF_ID,
)
from esphome.components import uart

CODEOWNERS = ["@niklasweber"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

dfrobot_mmwave_radar_ns = cg.esphome_ns.namespace("dfrobot_mmwave_radar")
DfrobotMmwaveRadarComponent = dfrobot_mmwave_radar_ns.class_(
    "DfrobotMmwaveRadarComponent", cg.Component
)

# Actions
DfrobotMmwaveRadarPowerAction = dfrobot_mmwave_radar_ns.class_(
    "DfrobotMmwaveRadarPowerAction", automation.Action
)
DfrobotMmwaveRadarResetAction = dfrobot_mmwave_radar_ns.class_(
    "DfrobotMmwaveRadarResetAction", automation.Action
)
DfrobotMmwaveRadarSettingsAction = dfrobot_mmwave_radar_ns.class_(
    "DfrobotMmwaveRadarSettingsAction", automation.Action
)

DFROBOT_MMWAVE_RADAR_ID = "dfrobot_mmwave_radar_id"

DELAY_AFTER_DETECT = "delay_after_detect"
DELAY_AFTER_DISAPPEAR = "delay_after_disappear"
DETECTION_SEGMENTS = "detection_segments"
OUTPUT_LATENCY = "output_latency"
START_AFTER_POWER_ON = "start_after_power_on"
TURN_ON_LED = "turn_on_led"
FACTORY_RESET = "factory_reset"
PRESENCE_VIA_UART = "presence_via_uart"
SENSITIVITY = "sensitivity"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DfrobotMmwaveRadarComponent),
        }
    ).extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)


@automation.register_action(
    "dfrobot_mmwave_radar.start",
    DfrobotMmwaveRadarPowerAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DfrobotMmwaveRadarComponent),
        }
    ),
)
async def dfrobot_mmwave_radar_start_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_power(True))
    return var


@automation.register_action(
    "dfrobot_mmwave_radar.stop",
    DfrobotMmwaveRadarPowerAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DfrobotMmwaveRadarComponent),
        }
    ),
)
async def dfrobot_mmwave_radar_stop_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_power(False))
    return var


@automation.register_action(
    "dfrobot_mmwave_radar.reset",
    DfrobotMmwaveRadarResetAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DfrobotMmwaveRadarComponent),
        }
    ),
)
async def dfrobot_mmwave_radar_reset_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

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
        if isinstance(distance, core.Lambda):
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


def at_least_one_settings_option(config):
    """Make sure at least one option is defined

    All settings are optional, but at least one setting needs to be specified.
    Otherwise there is nothing to to and the action would be useless.
    """
    has_at_least_one_option = False

    if DETECTION_SEGMENTS in config:
        has_at_least_one_option = True
    if OUTPUT_LATENCY in config:
        has_at_least_one_option = True
    if START_AFTER_POWER_ON in config:
        has_at_least_one_option = True
    if TURN_ON_LED in config:
        has_at_least_one_option = True
    if FACTORY_RESET in config:
        has_at_least_one_option = True
    if PRESENCE_VIA_UART in config:
        has_at_least_one_option = True
    if SENSITIVITY in config:
        has_at_least_one_option = True

    if not has_at_least_one_option:
        raise cv.Invalid("At least one settings option is required")
    return config


MMWAVE_SETTINGS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DfrobotMmwaveRadarComponent),
        cv.Optional(FACTORY_RESET): cv.templatable(cv.boolean),
        cv.Optional(DETECTION_SEGMENTS): range_segment_list,
        cv.Optional(OUTPUT_LATENCY): {
            cv.Required(DELAY_AFTER_DETECT): cv.templatable(
                cv.All(
                    cv.positive_time_period,
                    cv.Range(max=core.TimePeriod(seconds=1638.375)),
                )
            ),
            cv.Required(DELAY_AFTER_DISAPPEAR): cv.templatable(
                cv.All(
                    cv.positive_time_period,
                    cv.Range(max=core.TimePeriod(seconds=1638.375)),
                )
            ),
        },
        cv.Optional(START_AFTER_POWER_ON): cv.templatable(cv.boolean),
        cv.Optional(TURN_ON_LED): cv.templatable(cv.boolean),
        cv.Optional(PRESENCE_VIA_UART): cv.templatable(cv.boolean),
        cv.Optional(SENSITIVITY): cv.templatable(cv.int_range(min=0, max=9)),
    }
).add_extra(at_least_one_settings_option)


@automation.register_action(
    "dfrobot_mmwave_radar.settings",
    DfrobotMmwaveRadarSettingsAction,
    MMWAVE_SETTINGS_SCHEMA,
)
async def dfrobot_mmwave_radar_settings_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    if FACTORY_RESET in config:
        template_ = await cg.templatable(config[FACTORY_RESET], args, int)
        cg.add(var.set_factory_reset(template_))
    if DETECTION_SEGMENTS in config:
        segments = config[DETECTION_SEGMENTS]

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
    if OUTPUT_LATENCY in config:
        template_ = await cg.templatable(
            config[OUTPUT_LATENCY][DELAY_AFTER_DETECT], args, float
        )
        if isinstance(template_, cv.TimePeriod):
            template_ = template_.total_milliseconds / 1000
        cg.add(var.set_delay_after_detect(template_))

        template_ = await cg.templatable(
            config[OUTPUT_LATENCY][DELAY_AFTER_DISAPPEAR], args, float
        )
        if isinstance(template_, cv.TimePeriod):
            template_ = template_.total_milliseconds / 1000
        cg.add(var.set_delay_after_disappear(template_))
    if START_AFTER_POWER_ON in config:
        template_ = await cg.templatable(config[START_AFTER_POWER_ON], args, int)
        cg.add(var.set_start_after_power_on(template_))
    if TURN_ON_LED in config:
        template_ = await cg.templatable(config[TURN_ON_LED], args, int)
        cg.add(var.set_turn_on_led(template_))
    if PRESENCE_VIA_UART in config:
        template_ = await cg.templatable(config[PRESENCE_VIA_UART], args, int)
        cg.add(var.set_presence_via_uart(template_))
    if SENSITIVITY in config:
        template_ = await cg.templatable(config[SENSITIVITY], args, int)
        cg.add(var.set_sensitivity(template_))

    return var
