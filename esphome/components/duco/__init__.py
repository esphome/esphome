import esphome.codegen as cg
from esphome.components import time, uart
import esphome.config_validation as cv
from esphome.const import CONF_DISCOVERY, CONF_ID, CONF_TIME, CONF_TIME_ID
from esphome.core import ID
from esphome.types import ConfigType

CODEOWNERS = ["@kokx"]
DEPENDENCIES = ["uart"]
DOMAIN = "duco"
MULTI_CONF = True

CONF_DISABLE_CRC = "disable_crc"
CONF_DUCO_ID = "duco_id"

duco_ns = cg.esphome_ns.namespace("duco")
Duco = duco_ns.class_("Duco", cg.Component, uart.UARTDevice)
# Base of every class that sends requests through the hub
DucoDevice = duco_ns.class_(
    "DucoDevice", cg.PollingComponent, cg.Parented.template(Duco)
)
DucoDiscovery = duco_ns.class_("DucoDiscovery", DucoDevice)
DucoTime = duco_ns.class_("DucoTime", cg.PollingComponent, cg.Parented.template(Duco))

# Entities of the platforms take the hub with this
DUCO_COMPONENT_SCHEMA = cv.Schema({cv.GenerateID(CONF_DUCO_ID): cv.use_id(Duco)})

DISCOVERY_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(DucoDiscovery)}).extend(
    cv.COMPONENT_SCHEMA
)

TIME_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DucoTime),
        cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
    }
).extend(cv.polling_component_schema("15min"))

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Duco),
            cv.Optional(CONF_DISABLE_CRC, default=False): cv.boolean,
            cv.Optional(CONF_DISCOVERY): DISCOVERY_SCHEMA,
            cv.Optional(CONF_TIME): TIME_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    DOMAIN,
    baud_rate=57600,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


_request_device_slot = cg.slot_counter("DUCO_DEVICE_COUNT")


async def register_duco_device(var: cg.MockObj, config: ConfigType, hub_id: ID) -> None:
    """Register a device that sends requests, and give it an open request slot on its hub."""
    await cg.register_component(var, config)
    hub = await cg.get_variable(hub_id)
    await cg.register_parented(var, hub)
    _request_device_slot(str(hub_id))
    cg.add(hub.register_device(var))


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config)
    cg.add(var.set_disable_crc(config[CONF_DISABLE_CRC]))

    if (discovery := config.get(CONF_DISCOVERY)) is not None:
        discovery_var = cg.new_Pvariable(discovery[CONF_ID])
        await register_duco_device(discovery_var, discovery, config[CONF_ID])

    if (time_config := config.get(CONF_TIME)) is not None:
        clock = await cg.get_variable(time_config[CONF_TIME_ID])
        time_var = cg.new_Pvariable(time_config[CONF_ID], clock)
        await cg.register_component(time_var, time_config)
        await cg.register_parented(time_var, var)
