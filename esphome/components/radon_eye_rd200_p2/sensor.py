import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, ble_client

from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_BECQUEREL_PER_CUBIC_METER,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    CONF_ID,
    CONF_RADON,
    CONF_TEMPERATURE,
    CONF_HUMIDITY,
    ICON_RADIOACTIVE,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
)

DEPENDENCIES = ["ble_client"]

radon_eye_rd200_p2_ns = cg.esphome_ns.namespace("radon_eye_rd200_p2")
RadonEyeRD200P2 = radon_eye_rd200_p2_ns.class_(
    "RadonEyeRD200P2", cg.PollingComponent, ble_client.BLEClientNode
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RadonEyeRD200P2),
            cv.Optional(CONF_RADON): sensor.sensor_schema(
                unit_of_measurement=UNIT_BECQUEREL_PER_CUBIC_METER,
                icon=ICON_RADIOACTIVE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("5min"))
    .extend(ble_client.BLE_CLIENT_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await ble_client.register_ble_node(var, config)

    if CONF_RADON in config:
        sens = await sensor.new_sensor(config[CONF_RADON])
        cg.add(var.set_radon(sens))
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))
    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity(sens))
