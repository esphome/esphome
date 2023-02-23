import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID
from .. import (
    lg_uart_ns,
    LG_UART_CLIENT_SCHEMA,
    CONF_LG_UART_CMD,
    CONF_DECODE_BASE,
    CONF_DECODE_BASE_TYPES,
    register_lg_uart_child,
    validate_uart_cmd_len,
    CODEOWNERS as co,
)

CODEOWNERS = co
DEPENDENCIES = ["lg_uart"]

LGUartSensor = lg_uart_ns.class_("LGUartSensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    sensor.SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(LGUartSensor),
            # TODO: length? It seems like all LG commands are two chars?
            cv.Required(CONF_LG_UART_CMD): cv.string_strict,
            # TODO: can I do this as just a number rather than a string?
            cv.Optional(CONF_DECODE_BASE, default=10): cv.one_of(
                *CONF_DECODE_BASE_TYPES, int=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(LG_UART_CLIENT_SCHEMA),
    validate_uart_cmd_len,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    await register_lg_uart_child(var, config)
    # The command chars
    cg.add(var.set_cmd(config[CONF_LG_UART_CMD]))
    cg.add(var.set_encoding_base(config[CONF_DECODE_BASE]))
