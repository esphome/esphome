import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE, STATE_CLASS_MEASUREMENT, UNIT_MILLISECOND
from esphome.types import ConfigType

from .. import CONF_SENDSPIN_ID, SendspinHub, request_metadata_support, sendspin_ns

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["sendspin"]

SendspinTrackProgressSensor = sendspin_ns.class_(
    "SendspinTrackProgressSensor",
    sensor.Sensor,
    cg.PollingComponent,
)
SendspinTrackDurationSensor = sendspin_ns.class_(
    "SendspinTrackDurationSensor",
    sensor.Sensor,
    cg.Component,
)


def _request_roles(config: ConfigType) -> ConfigType:
    """Request the necessary Sendspin roles for the sensor."""
    request_metadata_support()

    return config


_SENSOR_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
    unit_of_measurement=UNIT_MILLISECOND,
).extend(
    {
        cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "track_progress": _SENSOR_SCHEMA.extend(
                {cv.GenerateID(): cv.declare_id(SendspinTrackProgressSensor)}
            ).extend(cv.polling_component_schema("1s")),
            "track_duration": _SENSOR_SCHEMA.extend(
                {cv.GenerateID(): cv.declare_id(SendspinTrackDurationSensor)}
            ).extend(cv.COMPONENT_SCHEMA),
        },
        key=CONF_TYPE,
    ),
    cv.only_on_esp32,
    _request_roles,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SENDSPIN_ID])
    await sensor.register_sensor(var, config)
