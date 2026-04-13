import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIMEOUT
from esphome.core import CORE

watchdog_ns = cg.esphome_ns.namespace("watchdog")
WatchdogManagerComponent = watchdog_ns.class_("WatchdogManagerComponent", cg.Component)

CODEOWNERS = ["@oarcher"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WatchdogManagerComponent),
        cv.Optional(CONF_TIMEOUT): cv.All(
            cv.Any(cv.only_on_esp32, cv.only_on_rp2040),
            cv.positive_not_null_time_period,
            cv.positive_time_period_seconds,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if timeout_s := config.get(CONF_TIMEOUT):
        if CORE.is_esp32:
            add_idf_sdkconfig_option(
                "CONFIG_ESP_TASK_WDT_TIMEOUT_S", timeout_s.total_seconds
            )
        else:
            cg.add(var.set_timeout(timeout_s.total_milliseconds))
