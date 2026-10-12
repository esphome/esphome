import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import OpenTherm42Hub
from ..const import CONF_OPENTHERM42_ID, CONF_SWEEP_HAD_ERRORS

# Each entry maps to (device_class, entity_category).
DESCRIPTIONS: dict[str, tuple[str | None, str | None]] = {
    # Synthetic diagnostic entity (not a real spec data-id) -- deliberately not given the
    # sensor_and_informational_data_* prefix used elsewhere in this file, since that names a real
    # spec chapter (§5.3.4 Class 4) this entity has nothing to do with. True if any conversation
    # during the most recently completed sweep (see hub.h's Entry/sweep_length_passes_) was rejected
    # or failed at the datalink level -- the kind of thing that otherwise only ever shows up as a
    # log line. "problem" fits directly: on means something went wrong during the last sweep.
    CONF_SWEEP_HAD_ERRORS: (
        "problem",
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

TYPES: dict[str, cv.Schema] = {
    marker: binary_sensor.binary_sensor_schema(
        device_class=device_class or cv.UNDEFINED,
        entity_category=entity_category or cv.UNDEFINED,
    )
    for marker, (device_class, entity_category) in DESCRIPTIONS.items()
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
            var = await binary_sensor.new_binary_sensor(marker_config)
            cg.add(getattr(hub, f"set_{marker}_binary_sensor")(var))
