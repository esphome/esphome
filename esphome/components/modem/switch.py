import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_MODEL
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_MODEM_ID, GNSS_MODELS, ModemComponent, get_model

DEPENDENCIES = ["modem"]

CONF_GNSS = "gnss"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MODEM_ID): cv.use_id(ModemComponent),
        cv.Optional(CONF_GNSS): switch.switch_schema(
            switch.ApplySwitch,
            icon="mdi:satellite-variant",
            block_inverted=True,
            default_restore_mode="RESTORE_DEFAULT_OFF",
        ),
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    if CONF_GNSS in config:
        model = fv.get_declaring_config(config[CONF_MODEM_ID])[CONF_MODEL]
        if model not in GNSS_MODELS:
            raise cv.Invalid(
                f"GNSS is not supported on {model}; supported models: {', '.join(GNSS_MODELS)}"
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MODEM_ID])
    switches = switch.sub_switches(config, parent=hub)
    if (
        await switches(
            CONF_GNSS, hub.set_gnss_switch, apply="set_gnss(state)", publish="after"
        )
        is not None
    ):
        cg.add_define("USE_MODEM_GNSS_SWITCH")
        cg.add(hub.set_gnss_command(GNSS_MODELS[get_model()]))
