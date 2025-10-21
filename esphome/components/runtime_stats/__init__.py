"""
Runtime statistics component for ESPHome.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@bdraco"]

CONF_LOG_INTERVAL = "log_interval"

runtime_stats_ns = cg.esphome_ns.namespace("runtime_stats")
RuntimeStatsCollector = runtime_stats_ns.class_("RuntimeStatsCollector")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RuntimeStatsCollector),
        cv.Optional(
            CONF_LOG_INTERVAL, default="60s"
        ): cv.positive_time_period_milliseconds,
    }
)


async def to_code(config):
    """Generate code for the runtime statistics component."""
    # Define USE_RUNTIME_STATS when this component is used
    cg.add_define("USE_RUNTIME_STATS")

    # Create the runtime stats instance (constructor sets global_runtime_stats)
    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_log_interval(config[CONF_LOG_INTERVAL]))
