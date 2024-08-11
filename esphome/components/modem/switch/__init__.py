import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH
import esphome.final_validate as fv

from .. import CONF_MODEL, CONF_MODEM, final_validate_platform, modem_ns

CODEOWNERS = ["@oarcher"]

AUTO_LOAD = []

DEPENDENCIES = ["modem"]

IS_PLATFORM_COMPONENT = True

CONF_GNSS = "gnss"
CONF_GNSS_COMMAND = "gnss_command"

ICON_SATELLITE = "mdi:satellite-variant"

GnssSwitch = modem_ns.class_("GnssSwitch", switch.Switch, cg.Component)

# SIM70xx doesn't support AT+CGNSSINFO, so gnss is not available
MODEM_MODELS_GNSS_COMMAND = {"SIM7600": "AT+CGPS", "SIM7670": "AT+CGNSSPWR"}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_GNSS): switch.switch_schema(
            GnssSwitch,
            block_inverted=True,
            device_class=DEVICE_CLASS_SWITCH,
            icon=ICON_SATELLITE,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate_gnss(config):
    if config.get(CONF_GNSS, None):
        modem_config = fv.full_config.get().get(CONF_MODEM)
        modem_model = modem_config.get(CONF_MODEL, None)
        if modem_model not in MODEM_MODELS_GNSS_COMMAND:
            raise cv.Invalid(f"GNSS not supported for modem '{modem_model}'.")
        config[CONF_GNSS_COMMAND] = MODEM_MODELS_GNSS_COMMAND[modem_model]
    return config


FINAL_VALIDATE_SCHEMA = cv.All(final_validate_platform, _final_validate_gnss)


async def to_code(config):
    if gnss_config := config.get(CONF_GNSS):
        var = await switch.new_switch(gnss_config)
        await cg.register_component(var, config)
        cg.add(var.set_command(config[CONF_GNSS_COMMAND]))
