import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome import core
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
DfrobotMmwaveRadarDetRangeCfgAction = dfrobot_mmwave_radar_ns.class_(
    "DfrobotMmwaveRadarDetRangeCfgAction", automation.Action
)


DELAY_AFTER_DETECT = "delay_after_detect"
DELAY_AFTER_DISAPPEAR = "delay_after_disappear"
SEGMENTS = "segments"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DfrobotMmwaveRadarComponent),
            cv.Optional(DELAY_AFTER_DETECT, default="2.5s"): cv.All(
                cv.positive_time_period, cv.Range(max=core.TimePeriod(seconds=1638.375))
            ),
            cv.Optional(DELAY_AFTER_DISAPPEAR, default="10s"): cv.All(
                cv.positive_time_period, cv.Range(max=core.TimePeriod(seconds=1638.375))
            ),
        }
    ).extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(
        var.set_delay_after_detect(config[DELAY_AFTER_DETECT].total_milliseconds / 1000)
    )
    cg.add(
        var.set_delay_after_disappear(
            config[DELAY_AFTER_DISAPPEAR].total_milliseconds / 1000
        )
    )


def range_segment_list(input):
    """Validate input is a list of ranges which can be used to configure the dfrobot mmwave radar

    A list of segments should be provided. A minimum of one segment is required and a maximum of
    four segments is allowed. A segment describes a range of distances. E.g. from 0mm to 1m.
    The distances need to be defined in an ascending order and they cannot contain / intersect
    each other.
    """
    cv.check_not_templatable(input)

    # Make sure input is always a list
    if input is None or (isinstance(input, dict) and not input):
        input = []
    elif not isinstance(input, list):
        input = [input]

    if len(input) < 1:
        raise cv.Invalid("At least one segment needs to be specified")
    if len(input) > 4:
        raise cv.Invalid("Four segments can be specified at max")

    largest_distance = -1
    for segment in input:
        # Check if two positive distances are defined, separated by '-'
        match = re.match(r"^([+. \w]*)(\s*-\s*)([+. \w]*)$", segment)
        if match is None:
            raise cv.Invalid(
                f'Invalid range "{segment}". '
                'Specify two positive distances separated by "-"'
            )

        # Check validity of each distance
        distances = segment.split("-")
        distances = [s.strip() for s in distances]

        if not len(distances) == 2:
            raise cv.Invalid("Two distances must be specified!")

        for distance in distances:
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
            distances[distances.index(distance)] = m

        # Overwrite input with parsed and separated distances
        input[input.index(segment)] = distances

    return input


MMWAVE_DET_RANGE_CFG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DfrobotMmwaveRadarComponent),
        cv.Required(SEGMENTS): range_segment_list,
    }
)


@automation.register_action(
    "dfrobot_mmwave_radar.det_range_cfg",
    DfrobotMmwaveRadarDetRangeCfgAction,
    MMWAVE_DET_RANGE_CFG_SCHEMA,
)
async def dfrobot_mmwave_radar_det_range_cfg_to_code(
    config, action_id, template_arg, args
):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    # template_ = await cg.templatable(config[CONF_RECIPIENT], args, cg.std_string)
    # cg.add(var.set_recipient(template_))
    # template_ = await cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    # cg.add(var.set_message(template_))
    return var
