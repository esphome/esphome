from esphome import automation
import esphome.codegen as cg
from esphome.components import uart

# Imported by name: the ld2450.binary_sensor platform module would shadow the binary_sensor package here
from esphome.components.binary_sensor import binary_sensor_schema, new_binary_sensor
from esphome.components.text import Text, register_text, text_schema
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_ON_DATA,
    CONF_THROTTLE,
    DEVICE_CLASS_OCCUPANCY,
    ENTITY_CATEGORY_CONFIG,
)
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@hareeshmu"]
MULTI_CONF = True

ld2450_ns = cg.esphome_ns.namespace("ld2450")
LD2450Component = ld2450_ns.class_("LD2450Component", cg.Component, uart.UARTDevice)
PolygonZone = ld2450_ns.class_("PolygonZone", Text)

CONF_LD2450_ID = "ld2450_id"
CONF_POLYGON = "polygon"
CONF_POLYGON_ZONES = "polygon_zones"
CONF_PRESENCE = "presence"

ICON_VECTOR_POLYGON = "mdi:vector-polygon"

_request_polygon_zone_slot = cg.slot_counter("LD2450_POLYGON_ZONE_COUNT")


def AUTO_LOAD(config: ConfigType | list[ConfigType] | None = None) -> list[str]:
    """Load the entity domains used by polygon zones only when a zone is configured."""
    configs = config if isinstance(config, list) else [config]
    # A falsy config is a tooling probe for the maximal set
    if not config or any(conf.get(CONF_POLYGON_ZONES) for conf in configs):
        return ["binary_sensor", "ld24xx", "text"]
    return ["ld24xx"]


POLYGON_ZONE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_POLYGON): text_schema(
            PolygonZone,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_VECTOR_POLYGON,
            mode="TEXT",
        ),
        cv.Required(CONF_PRESENCE): binary_sensor_schema(
            device_class=DEVICE_CLASS_OCCUPANCY,
        ),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2450Component),
            cv.Optional(CONF_THROTTLE): cv.invalid(
                f"{CONF_THROTTLE} has been removed; use per-sensor filters, instead"
            ),
            cv.Optional(CONF_ON_DATA): automation.validate_automation({}),
            cv.Optional(CONF_POLYGON_ZONES): cv.ensure_list(POLYGON_ZONE_SCHEMA),
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


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)
    for zone_conf in config.get(CONF_POLYGON_ZONES, []):
        presence = await new_binary_sensor(zone_conf[CONF_PRESENCE])
        polygon_conf = zone_conf[CONF_POLYGON]
        zone = cg.new_Pvariable(polygon_conf[CONF_ID], presence)
        await register_text(zone, polygon_conf)
        _request_polygon_zone_slot(str(var))
        cg.add(var.register_polygon_zone(zone))
