import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import cover, sensor
from esphome.const import (
    CONF_CLOSE_ACTION,
    CONF_CLOSE_DURATION,
    CONF_ID,
    CONF_OPEN_ACTION,
    CONF_OPEN_DURATION,
    CONF_STOP_ACTION,
    CONF_ASSUMED_STATE,
    CONF_FROM,
    CONF_TO,
)

time_based_ns = cg.esphome_ns.namespace("time_based")
TimeBasedCover = time_based_ns.class_("TimeBasedCover", cover.Cover, cg.Component)

CONF_HAS_BUILT_IN_ENDSTOP = "has_built_in_endstop"
CONF_PIECEWISE = "piecewise"

CONFIG_SCHEMA = cover.COVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TimeBasedCover),
        cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
        cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_PIECEWISE): cv.ensure_list(sensor.validate_datapoint),
        cv.Optional(CONF_HAS_BUILT_IN_ENDSTOP, default=False): cv.boolean,
        cv.Optional(CONF_ASSUMED_STATE, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


def evaluate_piecewise_coefficients(datapoints):
    """Note that datapoints should be properly ordered and non-degenerated."""
    print(datapoints)
    coefficients = []
    cuts = []

    # on first iteration, "last datapoint" is the first one (index zero)
    last_dp = datapoints[0]
    ldpi = last_dp[CONF_FROM]
    ldpj = last_dp[CONF_TO]

    for dp in datapoints[1:]:
        dpi = dp[CONF_FROM]
        dpj = dp[CONF_TO]
        cuts.append(dpi)

        # slope:
        # $$ \frac{y_b - y_a}{x_b - x_a} $$
        m = (dpj - ldpj) / (dpi - ldpi)
        # point:
        # $$ y - y_1 = m (x - x_1) $$
        n = -m * ldpi + ldpj
        # equation:
        # $$ y = mx + n $$
        coefficients.append([m, n])

        ldpi = dpi
        ldpj = dpj

    return coefficients, cuts


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)

    await automation.build_automation(
        var.get_stop_trigger(), [], config[CONF_STOP_ACTION]
    )

    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    await automation.build_automation(
        var.get_open_trigger(), [], config[CONF_OPEN_ACTION]
    )

    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    await automation.build_automation(
        var.get_close_trigger(), [], config[CONF_CLOSE_ACTION]
    )

    cg.add(var.set_has_built_in_endstop(config[CONF_HAS_BUILT_IN_ENDSTOP]))
    cg.add(var.set_assumed_state(config[CONF_ASSUMED_STATE]))

    if CONF_PIECEWISE in config:
        coefficients, cuts = evaluate_piecewise_coefficients(config[CONF_PIECEWISE])
        print(coefficients, cuts)

        cg.add(var.set_cuts(cuts))
        cg.add(var.set_coefficients(coefficients))
