from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_UART_ID
from esphome.core import CORE, ID
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import uart_ns, validate_raw_data

DEPENDENCIES = ["uart"]
DOMAIN = "uart_binary_sensor"

CONF_HUB_ID = "hub_id"

UARTBinarySensorHub = uart_ns.class_(
    "UARTBinarySensorHub", uart.UARTDevice, cg.Component
)

# Every sensor on a bus is one matcher of that bus's hub; the define is the largest hub
_request_matcher_slot = cg.slot_counter("UART_BINARY_SENSOR_COUNT")

CONFIG_SCHEMA = (
    # Initially off, so the first sequence after boot is an OFF to ON edge for on_press
    binary_sensor.binary_sensor_schema(binary_sensor.BinarySensorInitiallyOff)
    .extend(
        {
            cv.GenerateID(CONF_HUB_ID): cv.declare_id(UARTBinarySensorHub),
            # An empty pattern would match every byte; the hub's buffer index is a byte
            cv.Required(CONF_DATA): cv.All(
                validate_raw_data, cv.Length(min=1, max=255)
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
)


_validate_bus = uart.final_validate_device_schema("uart binary_sensor", require_rx=True)


def _one_hub_per_bus(config: ConfigType) -> ConfigType:
    """The first matching sensor takes the bytes and clears them, so a sequence that contains
    another on the same bus would leave one of them dead; and the sensors of a bus share the hub
    the first one creates, so only that one can name it and claim the bus's receive side."""
    data = bytes(config[CONF_DATA])
    first = True
    for other in fv.platform_entries(
        domain="binary_sensor",
        platform="uart",
        match={CONF_UART_ID: config[CONF_UART_ID]},
    ):
        if other is config:
            break  # Every sensor before this one on the bus has been seen
        first = False
        other_data = bytes(other[CONF_DATA])
        if other_data in data or data in other_data:
            raise cv.Invalid(
                f"Another uart binary_sensor on '{config[CONF_UART_ID]}' has data this pattern "
                "would match as well"
            )
        if config[CONF_HUB_ID].is_manual:
            raise cv.Invalid(
                f"Set hub_id on the first uart binary_sensor of '{config[CONF_UART_ID]}'; "
                "the others share its hub"
            )
    if first:
        _validate_bus(config)
    return config


FINAL_VALIDATE_SCHEMA = _one_hub_per_bus


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
