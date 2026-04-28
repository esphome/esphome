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


def _get_hubs() -> dict:  # uart_id -> (hub, matchers)
    return CORE.data.setdefault(DOMAIN, {})


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
    hubs = _get_hubs()
    uart_id = config[CONF_UART_ID]

    if uart_id not in hubs:
        hub = cg.new_Pvariable(config[CONF_HUB_ID])
        await cg.register_component(hub, config)
        await uart.register_uart_device(hub, config)
        hubs[uart_id] = (hub, [])
        CORE.add_job(_finalize_hubs)

    _, matchers = hubs[uart_id]
    var = await binary_sensor.new_binary_sensor(config)

    raw_data = config[CONF_DATA]
    if isinstance(raw_data, bytes):
        raw_data = [HexInt(x) for x in raw_data]

    data_var_id = ID(
        f"uart_binary_sensor_data_{config[CONF_ID].id}",
        is_declaration=True,
        type=cg.uint8,
    )
    data_var = cg.static_const_array(data_var_id, cg.ArrayInitializer(*raw_data))
    matchers.append((var, data_var, len(raw_data)))


@coroutine_with_priority(CoroPriority.FINAL)
async def _finalize_hubs():
    for hub, matchers in _get_hubs().values():
        max_matcher_len = max(data_len for _, _, data_len in matchers)
        cg.add(hub.setup_matchers(len(matchers)))
        for sensor_var, data_var, data_len in matchers:
            cg.add(hub.add_event_matcher(sensor_var, data_var, data_len))
        cg.add(hub.setup_buffer(max_matcher_len))
