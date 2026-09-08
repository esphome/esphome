import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@0hax"]
MULTI_CONF = True

teleinfo_ns = cg.esphome_ns.namespace("teleinfo")
TeleInfo = teleinfo_ns.class_("TeleInfo", cg.PollingComponent, uart.UARTDevice)

CONF_TELEINFO_ID = "teleinfo_id"
CONF_TAG_NAME = "tag_name"
CONF_HISTORICAL_MODE = "historical_mode"


TELEINFO_LISTENER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TELEINFO_ID): cv.use_id(TeleInfo),
        cv.Required(CONF_TAG_NAME): cv.string,
    }
)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TeleInfo),
            cv.Optional(CONF_HISTORICAL_MODE, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


def _final_validate(config: ConfigType) -> ConfigType:
    # Historical mode runs at 1200 baud, standard mode at 9600 baud.
    baud_rate = 1200 if config[CONF_HISTORICAL_MODE] else 9600
    uart.final_validate_device_schema(
        "teleinfo",
        baud_rate=baud_rate,
        data_bits=7,
        parity="EVEN",
        stop_bits=1,
    )(config)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_HISTORICAL_MODE])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
