from dataclasses import dataclass, field
import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
from esphome.components.const import CONF_ENABLED
import esphome.config_validation as cv
from esphome.const import CONF_ID
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
_request_cell_battery_slot = cg.slot_counter("PYLONTECH_CELL_BATTERY_COUNT")


@dataclass
class PylontechData:
    batteries: set[tuple[str, int]] = field(default_factory=set)
    cell_batteries: set[tuple[str, int]] = field(default_factory=set)


def _get_data() -> PylontechData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = PylontechData()
    return CORE.data[DOMAIN]


async def register_listener(
    config: ConfigType, listener: MockObj, *, cells: bool = False
) -> None:
    """Register a battery listener; the hub keeps one line slot per distinct battery.

    With cells, the hub also reads that battery's cells after every pwr reply and keeps
    one cell slot per distinct battery.
    """
    hub = await cg.get_variable(config[CONF_PYLONTECH_ID])
    hub_key = str(hub)
    _request_listener_slot(hub_key)
    data = _get_data()
    key = (hub_key, config[CONF_BATTERY])
    if key not in data.batteries:
        data.batteries.add(key)
        _request_battery_slot(hub_key)
    if cells and key not in data.cell_batteries:
        data.cell_batteries.add(key)
        _request_cell_battery_slot(hub_key)
        cg.add(hub.add_cell_battery(config[CONF_BATTERY]))
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

automation.register_apply_action(
    "pylontech.set_cell_polling",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(PylontechComponent),
            cv.Required(CONF_ENABLED): cv.templatable(cv.boolean),
        }
    ),
    automation.ApplyField(CONF_ENABLED, "set_cell_polling_enabled", cg.bool_),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_define("USE_LINE_READER")
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
