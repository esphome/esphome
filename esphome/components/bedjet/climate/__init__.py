import esphome.codegen as cg
from esphome.components import ble_client, climate
import esphome.config_validation as cv
from esphome.const import (
    CONF_HEAT_MODE,
    CONF_RECEIVE_TIMEOUT,
    CONF_TEMPERATURE_SOURCE,
    CONF_TIME_ID,
)

from .. import BEDJET_CLIENT_SCHEMA, bedjet_ns, register_bedjet_child

CODEOWNERS = ["@jhansche"]
DEPENDENCIES = ["bedjet"]

BedJetClimate = bedjet_ns.class_("BedJetClimate", climate.Climate, cg.PollingComponent)
BedjetHeatMode = bedjet_ns.enum("BedjetHeatMode")
BedjetTemperatureSource = bedjet_ns.enum("BedjetTemperatureSource")
BEDJET_HEAT_MODES = {
    "heat": BedjetHeatMode.HEAT_MODE_HEAT,
    "extended": BedjetHeatMode.HEAT_MODE_EXTENDED,
}
BEDJET_TEMPERATURE_SOURCES = {
    "outlet": BedjetTemperatureSource.TEMPERATURE_SOURCE_OUTLET,
    "ambient": BedjetTemperatureSource.TEMPERATURE_SOURCE_AMBIENT,
}

CONFIG_SCHEMA = (
    climate.climate_schema(BedJetClimate)
    .extend(
        {
            cv.Optional(CONF_HEAT_MODE, default="heat"): cv.enum(
                BEDJET_HEAT_MODES, lower=True
            ),
            cv.Optional(CONF_TEMPERATURE_SOURCE, default="ambient"): cv.enum(
                BEDJET_TEMPERATURE_SOURCES, lower=True
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
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await register_bedjet_child(var, config)

    cg.add(var.set_heating_mode(config[CONF_HEAT_MODE]))
    cg.add(var.set_temperature_source(config[CONF_TEMPERATURE_SOURCE]))
