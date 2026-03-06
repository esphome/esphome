import esphome.codegen as cg
from esphome.components import nmea
import esphome.config_validation as cv
import esphome.final_validate as fv

from .. import (
    CONF_MODEL,
    CONF_MODEM_ID,
    MODEM_COMPONENT_SCHEMA,
    final_validate_platform,
)

CODEOWNERS = ["@oarcher"]
DEPENDENCIES = ["nmea", "modem"]

CONF_GNSS_COMMAND = "gnss_command"

nmea_ns = cg.esphome_ns.namespace("nmea")
ModemNMEAComponent = nmea_ns.class_("ModemNMEAComponent", nmea.NMEAComponent)

MODEM_MODELS_GNSS_QUERY = {
    "SIM7600": {"command": "AT+CGNSSINFO"},
    # WARNING: some 7670 doesn't have gnss firmware support. Firmware version from ATI must end with '_F'
    "SIM7670": {"command": "AT+CGNSSINFO"},
    # SIM7080G cannot connect to cellular network and GPS positioning at the same time
    #    "SIM7080": {"command": "AT+CGNSINF"},
}

CONFIG_SCHEMA = cv.All(
    nmea.NMEA_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ModemNMEAComponent),
            cv.Optional(CONF_GNSS_COMMAND): cv.string,
        }
    )
    .extend(MODEM_COMPONENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
)


def _final_validate_modem_nmea(config):
    # Get modem model from modem config, and add CONF_GNSS_COMMAND to config if not specified
    fconf = fv.full_config.get()
    modem_path = fconf.get_path_for_id(config[CONF_MODEM_ID])[:-1]
    modem_config = fconf.get_config_for_path(modem_path)

    if modem_model := modem_config.get(CONF_MODEL, None):
        if modem_model not in MODEM_MODELS_GNSS_QUERY:
            raise cv.Invalid(
                f"NMEA not supported for modem '{modem_model}'. Supported models: {', '.join(MODEM_MODELS_GNSS_QUERY.keys())}"
            )

        # Set default GNSS command if not specified
        if CONF_GNSS_COMMAND not in config:
            config[CONF_GNSS_COMMAND] = MODEM_MODELS_GNSS_QUERY[modem_model]["command"]

    return config


FINAL_VALIDATE_SCHEMA = cv.All(final_validate_platform, _final_validate_modem_nmea)


async def to_code(config):
    var = await nmea.new_nmea(config)
    # NMEAComponent already inherits from UARTComponent - no register_uart_device needed
    cg.add(var.set_gnss_command(config[CONF_GNSS_COMMAND]))
