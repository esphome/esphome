from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.const import CONF_ON_DATA, CONF_THROTTLE
from esphome.types import ConfigType

AUTO_LOAD = ["ld24xx"]
DEPENDENCIES = ["uart"]
CODEOWNERS = ["@hareeshmu"]
DOMAIN = "ld2450"
MULTI_CONF = True

ld2450_ns = cg.esphome_ns.namespace("ld2450")
LD2450Component = ld2450_ns.class_("LD2450Component", cg.Component, uart.UARTDevice)


CONF_LD2450_ID = "ld2450_id"
CONF_LINK_TIMEOUT = "link_timeout"
CONF_POLYGON_ZONE_ID = "polygon_zone_id"
CONF_POLYGON_ZONES = "polygon_zones"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2450Component),
            cv.Optional(CONF_THROTTLE): cv.invalid(
                f"{CONF_THROTTLE} has been removed; use per-sensor filters, instead"
            ),
            cv.Optional(CONF_LINK_TIMEOUT, default="5s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(seconds=1),
                    max=cv.TimePeriod(hours=1),
                ),
            ),
            cv.Optional(CONF_ON_DATA): automation.validate_automation({}),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

LD2450BaseSchema = cv.Schema(
    {
        cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    },
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ld2450",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(CONF_ON_DATA, "add_on_data_callback"),
)

# Polygon zone sources build only when zones are configured
FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {
        "polygon.cpp": "LD2450_POLYGON_ZONE_COUNT",
        "polygon_zone.cpp": "LD2450_POLYGON_ZONE_COUNT",
    }
)


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config)
    cg.add(var.set_link_timeout(config[CONF_LINK_TIMEOUT].total_milliseconds))
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)
