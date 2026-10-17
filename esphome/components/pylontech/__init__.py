from dataclasses import dataclass, field
import logging

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@functionpointer"]
DEPENDENCIES = ["uart"]
DOMAIN = "pylontech"
MULTI_CONF = True

CONF_PYLONTECH_ID = "pylontech_id"
CONF_BATTERY = "battery"

pylontech_ns = cg.esphome_ns.namespace("pylontech")
PylontechComponent = pylontech_ns.class_(
    "PylontechComponent", cg.PollingComponent, uart.UARTDevice
)
PylontechBattery = pylontech_ns.class_("PylontechBattery")

CV_NUM_BATTERIES = cv.int_range(1, 16)

_request_listener_slot = cg.slot_counter("PYLONTECH_LISTENER_COUNT")
_request_battery_slot = cg.slot_counter("PYLONTECH_BATTERY_COUNT")


@dataclass
class PylontechData:
    batteries: set[tuple[str, int]] = field(default_factory=set)


def _get_data() -> PylontechData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = PylontechData()
    return CORE.data[DOMAIN]


async def register_listener(config: ConfigType, listener: MockObj) -> None:
    """Register a battery listener; the hub keeps one line slot per distinct battery."""
    hub = await cg.get_variable(config[CONF_PYLONTECH_ID])
    hub_key = str(hub)
    _request_listener_slot(hub_key)
    data = _get_data()
    if (key := (hub_key, config[CONF_BATTERY])) not in data.batteries:
        data.batteries.add(key)
        _request_battery_slot(hub_key)
    cg.add(hub.register_listener(listener))


PYLONTECH_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PYLONTECH_ID): cv.use_id(PylontechComponent),
        cv.Required(CONF_BATTERY): CV_NUM_BATTERIES,
    }
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PylontechComponent),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "pylontech",
    baud_rate=115200,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_LINE_READER")
    await uart.new_uart_component(config)
