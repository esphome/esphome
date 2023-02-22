import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID
from .. import (
    lg_uart_ns,
    LG_UART_CLIENT_SCHEMA,
    CONF_LG_UART_CMD,
    register_lg_uart_child,
    validate_uart_cmd_len,
    CODEOWNERS as co,
)

CODEOWNERS = co
DEPENDENCIES = ["lg_uart"]

LGUartSwitch = lg_uart_ns.class_("LGUartSwitch", switch.Switch, cg.PollingComponent)


CONFIG_SCHEMA = cv.All(
    switch.SWITCH_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(LGUartSwitch),
            # TODO: length? It seems like all LG commands are two chars?
            cv.Required(CONF_LG_UART_CMD): cv.string_strict,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(LG_UART_CLIENT_SCHEMA),
    validate_uart_cmd_len,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)
    await register_lg_uart_child(var, config)
    # The command chars
    cg.add(var.set_cmd(config[CONF_LG_UART_CMD]))
