import esphome.codegen as cg
from esphome.components import bluetooth_connection, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_RADON,
    CONF_RADON_LONG_TERM,
    ICON_RADIOACTIVE,
    STATE_CLASS_MEASUREMENT,
    UNIT_BECQUEREL_PER_CUBIC_METER,
)
from esphome.types import ConfigType

AUTO_LOAD = ["bluetooth_connection"]

radon_eye_rd200_ns = cg.esphome_ns.namespace("radon_eye_rd200")
RadonEyeRD200 = radon_eye_rd200_ns.class_("RadonEyeRD200", cg.PollingComponent)

_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_BECQUEREL_PER_CUBIC_METER,
    icon=ICON_RADIOACTIVE,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = bluetooth_connection.gatt_client_config_schema(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RadonEyeRD200),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_RADON): _SENSOR_SCHEMA,
            cv.Optional(CONF_RADON_LONG_TERM): _SENSOR_SCHEMA,
        }
    ).extend(cv.polling_component_schema("5min")),
    "radon_eye_rd200",
)


async def to_code(config: ConfigType) -> None:
    backend = await bluetooth_connection.new_gatt_backend(config)
    var = cg.new_Pvariable(config[CONF_ID], backend, config[CONF_MAC_ADDRESS].as_hex)
    await cg.register_component(var, config)

    if (radon := config.get(CONF_RADON)) is not None:
        sens = await sensor.new_sensor(radon)
        cg.add(var.set_radon(sens))
    if (radon_long_term := config.get(CONF_RADON_LONG_TERM)) is not None:
        sens = await sensor.new_sensor(radon_long_term)
        cg.add(var.set_radon_long_term(sens))
