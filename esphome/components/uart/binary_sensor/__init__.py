from collections import defaultdict
from dataclasses import dataclass, field

import esphome.codegen as cg
from esphome.components import binary_sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_ID, CONF_UART_ID
from esphome.core import CORE, ID, CoroPriority, HexInt, coroutine_with_priority

from .. import uart_ns, validate_raw_data

DEPENDENCIES = ["uart"]
DOMAIN = "uart_binary_sensor"

CONF_HUB_ID = "hub_id"

UARTBinarySensor = uart_ns.class_("UARTBinarySensor", uart.UARTDevice, cg.Component)


@dataclass
class UARTBinarySensorData:
    hub_by_uart: dict = field(default_factory=dict)
    matcher_count_by_uart: defaultdict = field(default_factory=lambda: defaultdict(int))
    max_matcher_len_by_uart: defaultdict = field(
        default_factory=lambda: defaultdict(int)
    )


def _get_data() -> UARTBinarySensorData:
    return CORE.data.setdefault(DOMAIN, UARTBinarySensorData())


CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema()
    .extend(
        {
            cv.GenerateID(CONF_HUB_ID): cv.declare_id(UARTBinarySensor),
            cv.Required(CONF_DATA): validate_raw_data,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    component_data = _get_data()
    uart_id = config[CONF_UART_ID]

    component_data.matcher_count_by_uart[uart_id] += 1

    if uart_id not in component_data.hub_by_uart:
        hub = cg.new_Pvariable(config[CONF_HUB_ID])
        await cg.register_component(hub, config)
        await uart.register_uart_device(hub, config)
        component_data.hub_by_uart[uart_id] = hub
    else:
        hub = component_data.hub_by_uart[uart_id]

    var = await binary_sensor.new_binary_sensor(config)

    raw_data = config[CONF_DATA]
    if isinstance(raw_data, bytes):
        raw_data = [HexInt(x) for x in raw_data]

    data_len = len(raw_data)
    component_data.max_matcher_len_by_uart[uart_id] = max(
        component_data.max_matcher_len_by_uart[uart_id], data_len
    )

    data_var_id = ID(
        f"uart_binary_sensor_data_{config[CONF_ID].id}",
        is_declaration=True,
        type=cg.uint8,
    )
    data_var = cg.static_const_array(data_var_id, cg.ArrayInitializer(*raw_data))
    cg.add(hub.add_event_matcher(var, data_var, data_len))


@coroutine_with_priority(CoroPriority.FINAL)
async def to_code_(config):
    component_data = _get_data()
    for uart_id, hub in component_data.hub_by_uart.items():
        matcher_count = component_data.matcher_count_by_uart[uart_id]
        max_matcher_len = component_data.max_matcher_len_by_uart[uart_id]
        cg.add(hub.matchers_.init(matcher_count))
        cg.add(hub.setup_buffer(max_matcher_len))
