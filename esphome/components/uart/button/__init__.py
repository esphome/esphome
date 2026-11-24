from esphome import automation
from esphome.components import button, uart
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_UART_ID
from esphome.types import ConfigType

from .. import validate_raw_payload

CODEOWNERS = ["@ssieb"]

DEPENDENCIES = ["uart"]

CONFIG_SCHEMA = (
    button.button_schema(button.ApplyButton)
    .extend(
        {
            cv.Required(CONF_DATA): validate_raw_payload,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    data = config[CONF_DATA]
    table = automation.progmem_bytes("uart_data", data)
    await button.sub_buttons(config, parent=config[CONF_UART_ID]).create(
        config, None, apply=f"write_array_progmem({table}, {len(data)})"
    )
