import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_prj_conf
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLOCK,
    CONF_DEVICE,
    CONF_FRAGMENTATION,
    CONF_FREE,
    CONF_ID,
    CONF_LOOP_TIME,
    PlatformFramework,
)
from esphome.core import CORE

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["logger"]

CONF_DEBUG_ID = "debug_id"
debug_ns = cg.esphome_ns.namespace("debug")
DebugComponent = debug_ns.class_("DebugComponent", cg.PollingComponent)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DebugComponent),
            cv.Optional(CONF_DEVICE): cv.invalid(
                "The 'device' option has been moved to the 'debug' text_sensor component"
            ),
            cv.Optional(CONF_FREE): cv.invalid(
                "The 'free' option has been moved to the 'debug' sensor component"
            ),
            cv.Optional(CONF_BLOCK): cv.invalid(
                "The 'block' option has been moved to the 'debug' sensor component"
            ),
            cv.Optional(CONF_FRAGMENTATION): cv.invalid(
                "The 'fragmentation' option has been moved to the 'debug' sensor component"
            ),
            cv.Optional(CONF_LOOP_TIME): cv.invalid(
                "The 'loop_time' option has been moved to the 'debug' sensor component"
            ),
        }
    ).extend(cv.polling_component_schema("60s")),
)


async def to_code(config):
    if CORE.using_zephyr:
        zephyr_add_prj_conf("HWINFO", True)
        # gdb thread support
        zephyr_add_prj_conf("DEBUG_THREAD_INFO", True)
        # RTT
        zephyr_add_prj_conf("USE_SEGGER_RTT", True)
        zephyr_add_prj_conf("RTT_CONSOLE", True)
        zephyr_add_prj_conf("LOG", True)
        zephyr_add_prj_conf("LOG_BLOCK_IN_THREAD", True)
        zephyr_add_prj_conf("LOG_BUFFER_SIZE", 4096)
        zephyr_add_prj_conf("SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL", True)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_DEBUG")


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "debug_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "debug_esp8266.cpp": {PlatformFramework.ESP8266_ARDUINO},
        "debug_host.cpp": {PlatformFramework.HOST_NATIVE},
        "debug_rp2040.cpp": {PlatformFramework.RP2040_ARDUINO},
        "debug_libretiny.cpp": {
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
        },
        "debug_zephyr.cpp": {PlatformFramework.NRF52_ZEPHYR},
    }
)
