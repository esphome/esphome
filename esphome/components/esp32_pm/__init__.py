import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MIN_FREQ_MHZ, CONF_MAX_FREQ_MHZ, CONF_TICKLESS
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.core import CORE

DEPENDENCIES = ["esp32"]

pm_ns = cg.esphome_ns.namespace("esp32_pm")
PM = pm_ns.class_("ESP32PowerManagement", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PM),
        cv.Optional(CONF_MIN_FREQ_MHZ, default=40): cv.uint16_t,
        cv.Optional(CONF_MAX_FREQ_MHZ, default=240): cv.uint16_t,
        cv.Optional(CONF_TICKLESS, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_PM_ENABLE", True)
        add_idf_sdkconfig_option("CONFIG_FREERTOS_USE_TICKLESS_IDLE", True)
        add_idf_sdkconfig_option("CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP", 3)
    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_freq(config[CONF_MIN_FREQ_MHZ], config[CONF_MAX_FREQ_MHZ]))
    cg.add(var.set_tickless(config[CONF_TICKLESS]))

    yield cg.register_component(var, config)

    cg.add_define("USE_PM", True)
