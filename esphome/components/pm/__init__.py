import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.core import CORE

DEPENDENCIES = ["esp32"]

pm_ns = cg.esphome_ns.namespace("pm")
PM = pm_ns.class_("PM", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PM),
        cv.Optional("min_freq_mhz", default=40): cv.uint16_t,
        cv.Optional("max_freq_mhz", default=240): cv.uint16_t,
        cv.Optional("tickless", default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_PM_ENABLE", True)
        add_idf_sdkconfig_option("CONFIG_FREERTOS_USE_TICKLESS_IDLE", True)
        add_idf_sdkconfig_option("CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP", 3)
    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_freq(config["min_freq_mhz"], config["max_freq_mhz"]))
    cg.add(var.set_tickless(config["tickless"]))

    yield cg.register_component(var, config)

    cg.add_define("USE_PM", True)
