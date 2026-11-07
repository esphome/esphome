import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from .. import CONF_ESPECTRE_ID, ESPectreComponent, espectre_ns

DEPENDENCIES = ["espectre"]

CONF_MOVEMENT = "movement"
CONF_DIAGNOSTICS = "diagnostics"
CONF_GENERATOR_RATE = "generator_rate"
CONF_TRAFFIC_TX_RATE = "traffic_tx_rate"
CONF_TRAFFIC_RX_RATE = "traffic_rx_rate"
CONF_CSI_ACCEPTED_RATE = "csi_accepted_rate"
CONF_CSI_OCCUPANCY = "csi_occupancy"
UNIT_PACKETS_PER_SECOND = "pps"

DiagnosticsUpdater = espectre_ns.class_("DiagnosticsUpdater", cg.PollingComponent)


def _diagnostic_schema(unit: str, accuracy_decimals: int) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement=unit,
        accuracy_decimals=accuracy_decimals,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )


DIAGNOSTIC_SENSORS = {
    CONF_GENERATOR_RATE: _diagnostic_schema(UNIT_PACKETS_PER_SECOND, 1),
    CONF_TRAFFIC_TX_RATE: _diagnostic_schema(UNIT_PACKETS_PER_SECOND, 1),
    CONF_TRAFFIC_RX_RATE: _diagnostic_schema(UNIT_PACKETS_PER_SECOND, 1),
    CONF_CSI_ACCEPTED_RATE: _diagnostic_schema(UNIT_PACKETS_PER_SECOND, 1),
    CONF_CSI_OCCUPANCY: _diagnostic_schema(UNIT_PERCENT, 0),
}

# Diagnostics are rarely watched, so they publish only on request unless an interval is set.
DIAGNOSTICS_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DiagnosticsUpdater),
            **{cv.Optional(key): schema for key, schema in DIAGNOSTIC_SENSORS.items()},
        }
    ).extend(cv.polling_component_schema("never")),
    cv.has_at_least_one_key(*DIAGNOSTIC_SENSORS),
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ESPECTRE_ID): cv.use_id(ESPectreComponent),
            cv.Optional(CONF_MOVEMENT): sensor.sensor_schema(
                accuracy_decimals=3, state_class=STATE_CLASS_MEASUREMENT
            ),
            cv.Optional(CONF_DIAGNOSTICS): DIAGNOSTICS_SCHEMA,
        }
    ),
    cv.has_at_least_one_key(CONF_MOVEMENT, CONF_DIAGNOSTICS),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ESPECTRE_ID])
    await sensor.sub_sensors(config)(CONF_MOVEMENT, hub.set_movement_sensor)
    if (diagnostics_config := config.get(CONF_DIAGNOSTICS)) is not None:
        updater = cg.new_Pvariable(diagnostics_config[CONF_ID], hub)
        await cg.register_component(updater, diagnostics_config)
        diagnostic_sensors = sensor.sub_sensors(diagnostics_config)
        for key in DIAGNOSTIC_SENSORS:
            await diagnostic_sensors(key, getattr(updater, f"set_{key}_sensor"))
