import esphome.codegen as cg
import esphome.config_validation as cv
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

CONF_DET_RANGE_MIN = "detection_range_min"
CONF_DET_RANGE_MAX = "detection_range_max"
DELAY_AFTER_DETECT = "delay_after_detect"
DELAY_AFTER_DISAPPEAR = "delay_after_disappear"


def validate_ranges(config):
    if CONF_DET_RANGE_MIN in config and CONF_DET_RANGE_MAX in config:
        min_val = config[CONF_DET_RANGE_MIN]
        max_val = config[CONF_DET_RANGE_MAX]
        if min_val >= max_val:
            raise cv.Invalid(
                f"Range min value {min_val} must be smaller than range max value {max_val}"
            )
    return config


# The DFROBOT mmwave radar command interface does not accept a range directly in meters,
# but in multiples of 15cm. E.g. 4 * 15 = 60cm. So to set 60cm, 4 is used. To make the input
# more readable, the user can specify a metric distance and it is calculted internally.
# Here, it is validated that the input is specified in multiples of 15 and 25.
def validate_steps(config):
    delays = [DELAY_AFTER_DETECT, DELAY_AFTER_DISAPPEAR]
    for delay in delays:
        if delay in config:
            if config[delay].total_milliseconds % 25 != 0:
                raise cv.Invalid(
                    f"{delay} must be defined in steps of 25ms (e.g. 0ms or 25ms or 50ms)"
                )

    ranges = [CONF_DET_RANGE_MIN, CONF_DET_RANGE_MAX]
    for range in ranges:
        if range in config:
            if (config[range] * 100) % 15 != 0:
                raise cv.Invalid(
                    f"{range} must be defined in steps of 15cm (e.g. 0cm 15cm 30cm)"
                )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DfrobotMmwaveRadarComponent),
            cv.Optional(CONF_DET_RANGE_MIN, default="0m"): cv.All(
                cv.distance, cv.Range(min=0, max=9)
            ),
            cv.Optional(CONF_DET_RANGE_MAX, default="3m"): cv.All(
                cv.distance, cv.Range(min=0, max=9)
            ),
            cv.Optional(DELAY_AFTER_DETECT, default="2.5s"): cv.All(
                cv.positive_time_period, cv.Range(max=core.TimePeriod(seconds=1638.375))
            ),
            cv.Optional(DELAY_AFTER_DISAPPEAR, default="10s"): cv.All(
                cv.positive_time_period, cv.Range(max=core.TimePeriod(seconds=1638.375))
            ),
        }
    ).extend(uart.UART_DEVICE_SCHEMA),
    validate_ranges,
    validate_steps,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_detection_range_min(config[CONF_DET_RANGE_MIN]))
    cg.add(var.set_detection_range_max(config[CONF_DET_RANGE_MAX]))
    cg.add(
        var.set_delay_after_detect(config[DELAY_AFTER_DETECT].total_milliseconds / 1000)
    )
    cg.add(
        var.set_delay_after_disappear(
            config[DELAY_AFTER_DISAPPEAR].total_milliseconds / 1000
        )
    )
