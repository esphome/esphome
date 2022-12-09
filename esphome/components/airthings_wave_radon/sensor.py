import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, ble_client

from esphome.const import (
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    UNIT_CELSIUS,
    ICON_RADIOACTIVE,
    CONF_ID,
    CONF_RADON,
    CONF_RADON_LONG_TERM,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    UNIT_BECQUEREL_PER_CUBIC_METER,
    ICON_RADIATOR,
)

DEPENDENCIES = ["ble_client"]

airthings_wave_radon_ns = cg.esphome_ns.namespace("airthings_wave_radon")
AirthingsWaveRadon = airthings_wave_radon_ns.class_(
    "AirthingsWaveRadon", cg.PollingComponent, ble_client.BLEClientNode
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AirthingsWaveRadon),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_RADON): sensor.sensor_schema(
                unit_of_measurement=UNIT_BECQUEREL_PER_CUBIC_METER,
                icon=ICON_RADIOACTIVE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RADON_LONG_TERM): sensor.sensor_schema(
                unit_of_measurement=UNIT_BECQUEREL_PER_CUBIC_METER,
                icon=ICON_RADIOACTIVE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
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

    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity(sens))
    if CONF_RADON in config:
        sens = await sensor.new_sensor(config[CONF_RADON])
        cg.add(var.set_radon(sens))
    if CONF_RADON_LONG_TERM in config:
        sens = await sensor.new_sensor(config[CONF_RADON_LONG_TERM])
        cg.add(var.set_radon_long_term(sens))
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))
