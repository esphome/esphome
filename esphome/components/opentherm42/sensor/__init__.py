import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_DURATION, ENTITY_CATEGORY_DIAGNOSTIC

from .. import OpenTherm42Hub
from ..const import CONF_OPENTHERM42_ID, CONF_PASS_DURATION, CONF_SWEEP_DURATION

TYPES: dict[str, cv.Schema] = {
    # Synthetic diagnostic, not an OpenTherm data-id -- deliberately not given the
    # sensor_and_informational_data_* prefix used throughout this file, since that names a real spec
    # chapter (§5.3.4 Class 4) this entity has nothing to do with. How long the most recently
    # completed sweep (a full pass attempting every configured id at least once) took, updated
    # continuously at the end of every sweep -- see hub.h's Entry/sweep_length_passes_ for the
    # pass-based scheduler this measures. Unconditionally scheduled by the hub itself regardless of
    # what else is configured, so no update_every field of its own.
    CONF_SWEEP_DURATION: sensor.sensor_schema(
        unit_of_measurement="ms",
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DURATION,
        state_class="measurement",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # Same nature as the sweep duration sensor above, but for a single pass (one full scan of every
    # configured id, however many of them happened to be due on it) rather than a whole sweep --
    # updated continuously at the end of every pass. Also unconditionally scheduled by the hub
    # itself, so no update_every field of its own.
    CONF_PASS_DURATION: sensor.sensor_schema(
        unit_of_measurement="ms",
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DURATION,
        state_class="measurement",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{cv.Optional(marker): schema for marker, schema in TYPES.items()},
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker in TYPES:
        if (marker_config := config.get(marker)) is not None:
            var = await sensor.new_sensor(marker_config)
            cg.add(getattr(hub, f"set_{marker}_sensor")(var))
