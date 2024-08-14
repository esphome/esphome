import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH
import esphome.final_validate as fv

from .. import (
    CONF_MODEL,
    CONF_MODEM_ID,
    MODEM_COMPONENT_SCHEMA,
    final_validate_platform,
    modem_ns,
)

CODEOWNERS = ["@oarcher"]

AUTO_LOAD = []

DEPENDENCIES = ["modem"]

IS_PLATFORM_COMPONENT = True

CONF_GNSS = "gnss"
CONF_GNSS_COMMAND = "gnss_command"  # will be set by _final_validate_gnss

ICON_SATELLITE = "mdi:satellite-variant"

GnssSwitch = modem_ns.class_("GnssSwitch", switch.Switch, cg.Component)

# SIM70xx doesn't support AT+CGNSSINFO, so gnss is not available
MODEM_MODELS_GNSS_COMMAND = {"SIM7600": "AT+CGPS", "SIM7670": "AT+CGNSSPWR"}


CONF_GNSS_SWITCH_ID = "gnss_switch_id"

GNSS_SWITCH_SCHEMA = cv.Schema(
    {cv.GenerateID(CONF_GNSS_SWITCH_ID): cv.use_id(GnssSwitch)}
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_GNSS): switch.switch_schema(
                GnssSwitch,
                block_inverted=True,
                device_class=DEVICE_CLASS_SWITCH,
                icon=ICON_SATELLITE,
            ),
        }
    )
    .extend(GNSS_SWITCH_SCHEMA)
    .extend(MODEM_COMPONENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


def _final_validate_gnss(config):
    # get modem model from modem config, and add CONF_GNSS_COMMAND to config
    if config.get(CONF_GNSS, None):
        fconf = fv.full_config.get()
        modem_path = fconf.get_path_for_id(config[CONF_MODEM_ID])[:-1]
        modem_config = fconf.get_config_for_path(modem_path)
        if modem_model := modem_config.get(CONF_MODEL, None):
            if modem_model not in MODEM_MODELS_GNSS_COMMAND:
                raise cv.Invalid(
                    f"GNSS not supported for modem '{modem_model}'. Supported models are %s",
                    ", ".join(MODEM_MODELS_GNSS_COMMAND.keys()),
                )

            # is it allowed to add config option?
            config[CONF_GNSS_COMMAND] = MODEM_MODELS_GNSS_COMMAND[modem_model]

    return config


FINAL_VALIDATE_SCHEMA = cv.All(final_validate_platform, _final_validate_gnss)


async def to_code(config):
    if gnss_config := config.get(CONF_GNSS):
        var = await switch.new_switch(gnss_config)
        await cg.register_component(var, config)
        cg.add(var.set_command(config[CONF_GNSS_COMMAND]))
