from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_UART_ID
from esphome.core import CORE, ID
from esphome.types import ConfigType

from .. import uart_ns, validate_raw_data

DEPENDENCIES = ["uart"]
DOMAIN = "uart_binary_sensor"

CONF_HUB_ID = "hub_id"

UARTBinarySensor = uart_ns.class_("UARTBinarySensor", uart.UARTDevice, cg.Component)

# Every sensor on a bus is one matcher of that bus's hub; the define is the largest hub
_request_matcher_slot = cg.slot_counter("UART_BINARY_SENSOR_COUNT")

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema()
    .extend(
        {
            cv.GenerateID(CONF_HUB_ID): cv.declare_id(UARTBinarySensor),
            # An empty pattern would match every byte; the hub's buffer index is a byte
            cv.Required(CONF_DATA): cv.All(
                validate_raw_data, cv.Length(min=1, max=255)
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
)


def _hubs() -> dict[ID, cg.MockObj]:
    """The hub of each bus, keyed by the bus id."""
    return CORE.data.setdefault(DOMAIN, {})


async def _hub_for(config: ConfigType) -> cg.MockObj:
    """The one hub that reads the bus for every sensor on it, created by the first sensor."""
    hubs = _hubs()
    if (hub := hubs.get(config[CONF_UART_ID])) is None:
        hub = hubs[config[CONF_UART_ID]] = cg.new_Pvariable(config[CONF_HUB_ID])
        await cg.register_component(hub, config)
        await uart.register_uart_device(hub, config)
    return hub


async def to_code(config: ConfigType) -> None:
    hub = await _hub_for(config)
    var = await binary_sensor.new_binary_sensor(config)
    data = config[CONF_DATA]
    _request_matcher_slot(str(hub))
    cg.add(
        hub.add_event_matcher(
            var, automation.progmem_bytes("uart_binary_sensor_data", data), len(data)
        )
    )
