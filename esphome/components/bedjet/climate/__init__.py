import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, ble_client
from esphome.const import (
    CONF_HEAT_MODE,
    CONF_ID,
    CONF_RECEIVE_TIMEOUT,
    CONF_TIME_ID,
)
from .. import (
    BEDJET_CLIENT_SCHEMA,
    bedjet_ns,
    register_bedjet_child,
)

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@jhansche"]
DEPENDENCIES = ["bedjet"]

BedJetClimate = bedjet_ns.class_("BedJetClimate", climate.Climate, cg.PollingComponent)
BedjetHeatMode = bedjet_ns.enum("BedjetHeatMode")
BEDJET_HEAT_MODES = {
    "heat": BedjetHeatMode.HEAT_MODE_HEAT,
    "extended": BedjetHeatMode.HEAT_MODE_EXTENDED,
}

CONFIG_SCHEMA = (
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(BedJetClimate),
            cv.Optional(CONF_HEAT_MODE, default="heat"): cv.enum(
                BEDJET_HEAT_MODES, lower=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(
        # TODO: remove compat layer.
        {
            cv.Optional(ble_client.CONF_BLE_CLIENT_ID): cv.invalid(
                "The 'ble_client_id' option has been removed. Please migrate "
                "to the new `bedjet_id` option in the `bedjet` component.\n"
                "See https://esphome.io/components/climate/bedjet.html"
            ),
            cv.Optional(CONF_TIME_ID): cv.invalid(
                "The 'time_id' option has been moved to the `bedjet` component."
            ),
            cv.Optional(CONF_RECEIVE_TIMEOUT): cv.invalid(
                "The 'receive_timeout' option has been moved to the `bedjet` component."
            ),
        }
    )
    .extend(BEDJET_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await register_bedjet_child(var, config)

    cg.add(var.set_heating_mode(config[CONF_HEAT_MODE]))
