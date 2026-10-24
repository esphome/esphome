import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import (
    CONF_MODEM_ID,
    GNSS_MODELS,
    ModemComponent,
    request_gnss_power,
    require_model,
)

DEPENDENCIES = ["modem"]

CONF_GNSS = "gnss"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MODEM_ID): cv.use_id(ModemComponent),
        cv.Required(CONF_GNSS): switch.switch_schema(
            switch.ApplySwitch,
            icon="mdi:satellite-variant",
            block_inverted=True,
            default_restore_mode="RESTORE_DEFAULT_OFF",
        ),
    }
)

FINAL_VALIDATE_SCHEMA = require_model((CONF_GNSS,), GNSS_MODELS)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MODEM_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(
        CONF_GNSS, hub.set_gnss_switch, apply="set_gnss(state)", publish="after"
    )
    request_gnss_power()
    cg.add_define("USE_MODEM_GNSS_SWITCH")
